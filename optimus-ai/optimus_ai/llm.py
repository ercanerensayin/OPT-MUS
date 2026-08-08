"""Claude tarafı: sohbet, kod üretimi ve yapılandırılmış planlama.

Buradaki her çağrı resmî `anthropic` SDK'sı üzerinden gider. Uzun çıktılar
(kod tabanı üretimi, sahne planları) için streaming kullanılır — aksi hâlde
büyük `max_tokens` değerleri HTTP zaman aşımına takılır.
"""

from __future__ import annotations

from collections.abc import Iterator
from typing import Any, TypeVar

import anthropic
from pydantic import BaseModel, Field

from .config import Config, get_config
from .errors import OptimusError

_ModelT = TypeVar("_ModelT", bound=BaseModel)

SYSTEM_CHAT = """Sen OPTIMUS'sun: kod yazan, uygulama ve oyun kuran, görsel ve
video üreten bir yapım asistanısın. Türkçe soruya Türkçe cevap ver.

Bildiğini bildiğin gibi söyle, bilmediğine "bilmiyorum" de; uydurma. Güncel
olaylar, fiyatlar veya sürüm ayrıntıları gibi değişken bilgilerde eğitim
verinin bir kesme tarihi olduğunu hatırla ve bunu belirt.

Cevapları soruya göre ölçekle: basit soruya doğrudan cevap, karmaşık işte
gereken derinlik. Başlık ve madde yığını yerine düz anlatım tercih et."""

SYSTEM_BUILDER = """Sen kıdemli bir yazılım mühendisisin. Verilen istekten
ÇALIŞAN bir proje üretiyorsun.

Kurallar:
- Her dosya eksiksiz olsun. "// buraya kodu yaz" gibi yer tutucu bırakma.
- Kurulum gerektirmeyen çözümü tercih et: oyun ve küçük araçlarda tek bir
  index.html (canvas + vanilla JS) çoğu zaman en iyisidir.
- Bağımlılık ekleyeceksen paket dosyasını (package.json / requirements.txt) da üret.
- Nasıl çalıştırılacağını anlatan kısa bir README.md ekle.
- Yollar proje köküne göreli olsun; mutlak yol veya `..` kullanma."""


class GeneratedFile(BaseModel):
    path: str = Field(description="Proje köküne göreli dosya yolu, ör. src/main.js")
    content: str = Field(description="Dosyanın tam içeriği")


class GeneratedProject(BaseModel):
    name: str = Field(description="Kısa, dosya adı olarak kullanılabilir proje adı")
    summary: str = Field(description="Projenin bir cümlelik özeti")
    entrypoint: str = Field(description="Çalıştırılacak dosya, ör. index.html")
    run_command: str = Field(description="Çalıştırma komutu; gerekmiyorsa boş bırak")
    files: list[GeneratedFile]


class Scene(BaseModel):
    index: int = Field(description="0'dan başlayan sahne sırası")
    prompt: str = Field(
        description=(
            "Video modeline verilecek İngilizce görsel betimleme. Kamera, ışık, "
            "stil ve konu içersin. Önceki sahneyle görsel süreklilik kur."
        )
    )
    caption: str = Field(description="Ekranda gösterilebilecek kısa Türkçe altyazı")
    seconds: float = Field(description="Bu sahnenin saniye cinsinden süresi")


class ScenePlan(BaseModel):
    title: str
    style: str = Field(description="Tüm sahnelerde tekrarlanacak ortak stil cümlesi")
    scenes: list[Scene]


class Brain:
    """Claude çağrılarını tek yerde toplayan ince sarmalayıcı."""

    def __init__(self, config: Config | None = None) -> None:
        self.config = config or get_config()
        # Anahtar verilmezse SDK ortamdan / `ant auth login` profilinden çözer.
        self._client = anthropic.Anthropic(
            api_key=self.config.anthropic_api_key or None
        )

    # -- sohbet -------------------------------------------------------------

    def chat_stream(
        self,
        messages: list[dict[str, Any]],
        *,
        system: str = SYSTEM_CHAT,
        effort: str | None = None,
    ) -> Iterator[str]:
        """Cevabı parça parça üretir; arayüz bunu canlı yazdırır."""
        try:
            with self._client.messages.stream(
                model=self.config.model,
                max_tokens=32000,
                system=system,
                thinking={"type": "adaptive"},
                output_config={"effort": effort or self.config.effort},
                messages=messages,
            ) as stream:
                for text in stream.text_stream:
                    yield text
                final = stream.get_final_message()
        except anthropic.APIError as exc:
            raise OptimusError(f"Claude çağrısı başarısız: {exc}") from exc
        except OSError as exc:
            raise OptimusError(f"Claude'a bağlanılamadı: {exc}") from exc

        if final.stop_reason == "refusal":
            yield "\n\n[Bu istek güvenlik nedeniyle yanıtlanmadı.]"
        elif final.stop_reason == "max_tokens":
            yield "\n\n[Cevap uzunluk sınırına takıldı; devam etmemi istersen söyle.]"

    def chat(self, prompt: str, *, system: str = SYSTEM_CHAT) -> str:
        return "".join(self.chat_stream([{"role": "user", "content": prompt}], system=system))

    # -- yapılandırılmış üretim --------------------------------------------

    def build_project(self, request: str, *, kind: str = "app") -> GeneratedProject:
        """İstekten çalışır bir proje (dosya listesi) üretir."""
        hint = {
            "game": "Bu bir OYUN. Tarayıcıda tek dosyayla açılıp oynanabilsin; "
            "oyun döngüsü, girdi kontrolü, skor ve bitiş ekranı olsun.",
            "app": "Bu bir UYGULAMA. Kullanılabilir bir arayüz ve gerçek işleyen "
            "mantık içersin.",
        }.get(kind, "")

        return self._parse(
            GeneratedProject,
            system=SYSTEM_BUILDER,
            prompt=f"{hint}\n\nİstek:\n{request}",
            max_tokens=64000,
        )

    def plan_scenes(
        self, prompt: str, *, total_seconds: float, segment_seconds: float
    ) -> ScenePlan:
        """Uzun bir video isteğini, model sınırına sığan sahnelere böler."""
        count = max(1, round(total_seconds / segment_seconds))
        instruction = (
            f"Aşağıdaki fikri {count} sahnelik bir video senaryosuna böl. "
            f"Her sahne yaklaşık {segment_seconds} saniye olsun ve toplam "
            f"{total_seconds:.0f} saniyeyi versin.\n\n"
            "Sahneler arka arkaya oynatıldığında akan tek bir video gibi "
            "durmalı: aynı karakterler, aynı palet, aynı kamera dili. Her "
            "sahnenin promptu kendi başına anlaşılır olsun — video modeli "
            "önceki sahneyi görmüyor.\n\n"
            f"Fikir:\n{prompt}"
        )
        plan = self._parse(
            ScenePlan,
            system="Sen bir video yönetmeni ve storyboard sanatçısısın.",
            prompt=instruction,
            max_tokens=32000,
        )
        for i, scene in enumerate(plan.scenes):
            scene.index = i
        return plan

    def _parse(
        self, schema: type[_ModelT], *, system: str, prompt: str, max_tokens: int
    ) -> _ModelT:
        """Şemaya uyan yapılandırılmış cevap alır.

        Streaming şart: proje üretimi gibi yüksek `max_tokens` isteyen
        çağrılar streaming olmadan SDK'nın 10 dakikalık koruma eşiğine takılıp
        hiç gönderilmiyor.
        """
        try:
            with self._client.messages.stream(
                model=self.config.model,
                max_tokens=max_tokens,
                system=system,
                thinking={"type": "adaptive"},
                messages=[{"role": "user", "content": prompt}],
                output_format=schema,
            ) as stream:
                response = stream.get_final_message()
        except anthropic.APIError as exc:
            raise OptimusError(f"Claude çağrısı başarısız: {exc}") from exc
        except OSError as exc:
            raise OptimusError(f"Claude'a bağlanılamadı: {exc}") from exc

        if response.stop_reason == "refusal":
            raise OptimusError("Model bu isteği güvenlik nedeniyle reddetti.")

        for block in response.content:
            parsed = getattr(block, "parsed_output", None)
            if parsed is not None:
                return parsed

        if response.stop_reason == "max_tokens":
            raise OptimusError(
                "Cevap uzunluk sınırına takıldı; isteği daraltıp tekrar dene."
            )
        raise OptimusError(
            f"Model beklenen yapıda cevap üretemedi (stop_reason={response.stop_reason})."
        )
