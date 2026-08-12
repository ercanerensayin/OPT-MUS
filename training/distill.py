#!/usr/bin/env python3
"""
Ogretmen modelden veri damitma (distillation).

Calisan OPT-MUS sunucusuna prompt bankasindaki istekleri gonderir. Sunucu
uretimi yapar ve her uretimi otomatik olarak training/data/generations.jsonl
dosyasina yazar. Yani bu betik, kendi modelinizi egitecek veriyi guclu bir
ogretmen modele urettirir.

Onemli: sunucunun guclu bir saglayiciyla calisiyor olmasi gerekir
(.env -> LLM_PROVIDER=anthropic gibi). Kendi kucuk modelinizle veri
uretmek ogrenme saglamaz.

Kullanim:
    npm start                                  # baska bir terminalde
    python3 training/distill.py --count 300 --concurrency 3
"""

from __future__ import annotations

import argparse
import itertools
import json
import random
import sys
import threading
import urllib.error
import urllib.request
from queue import Queue

# --------------------------------------------------------------------------
# Prompt bankasi: konu x detay birlesimiyle yuzlerce farkli istek uretilir.
# --------------------------------------------------------------------------

SITE_TOPICS = [
    "bir kahve dukkani", "bir dis klinigi", "bir yoga studyosu", "bir emlak ofisi",
    "bir fotografcinin portfolyosu", "bir muzik grubu", "bir yazilim ajansi",
    "bir spor salonu", "bir pastane", "bir hukuk burosu", "bir veteriner klinigi",
    "bir seyahat blogu", "bir el yapimi taki markasi", "bir kitap kulubu",
    "bir online kurs platformu", "bir restoran", "bir mimarlik ofisi",
    "bir cocuk kres", "bir dogal kozmetik markasi", "bir bisiklet magazasi",
]

SITE_DETAILS = [
    "tek sayfalik tanitim sitesi, hizmetler ve iletisim formu olsun",
    "fiyat tablosu ve sikca sorulan sorular bolumu olsun",
    "galeri ve musteri yorumlari bolumu olsun",
    "randevu formu ve calisma saatleri tablosu olsun",
    "koyu tema, buyuk kapak gorseli ve haber bulteni kaydi olsun",
    "hakkimizda, ekip kartlari ve harita bolumu olsun",
    "urun listesi, filtreleme ve sepet ozeti olsun",
    "blog listesi ve detay bolumu olsun",
]

APP_TASKS = [
    "yapilacaklar listesi uygulamasi, filtreleme ve localStorage ile kalicilik",
    "not defteri uygulamasi, arama ve etiketleme",
    "pomodoro zamanlayici, ayarlanabilir sure ve ses uyarisi",
    "kisisel butce takip paneli, kategori bazli ozet ve grafik",
    "markdown editoru, canli onizleme ve disa aktarma",
    "kanban panosu, surukle birak ile kolon degistirme",
    "hesap makinesi, klavye destegi ve gecmis listesi",
    "quiz uygulamasi, puanlama ve sonuc ekrani",
    "hava durumu paneli, sehir arama ve sahte veri",
    "sifre ureteci, uzunluk ve karakter secenekleri",
    "renk paleti ureteci, kopyalama ve kaydetme",
    "birim cevirici, uzunluk agirlik ve sicaklik sekmeleri",
    "flashcard calisma uygulamasi, deste yonetimi",
    "resim galerisi, lightbox ve klavye ile gezinme",
    "sohbet arayuzu taslagi, sahte cevaplar ve yazma animasyonu",
]

CODE_TASKS = [
    "Python ile bir klasordeki tum gorselleri toplu yeniden boyutlandiran betik",
    "Python ile CSV dosyasini temizleyip ozet istatistik ureten betik",
    "Node.js ile bir REST API: kullanici CRUD, dosya tabanli saklama",
    "Python ile bir web sitesinden baslik ve link toplayan kazima betigi",
    "Bash ile veritabani yedegi alip eski yedekleri temizleyen betik",
    "Python ile klasordeki dosyalari uzantiya gore siniflandiran arac",
    "JavaScript ile derinlemesine nesne karsilastiran ve testleri olan kutuphane",
    "Python ile log dosyalarini ayristirip hata ozeti cikaran arac",
    "Node.js ile bir komut satiri araci: argumanlarla dosya arama",
    "Python ile basit bir onbellekli HTTP istemci sarmalayicisi",
]

STYLE_HINTS = [
    "", "", "", "  Tasarim minimal ve ferah olsun.",
    "  Canli renkler ve gradyanlar kullan.", "  Kurumsal ve sade bir dil kullan.",
    "  Animasyonlar ve gecisler ekle.", "  Erisilebilirlige ozellikle dikkat et.",
]


def build_prompt_bank(seed: int) -> list[tuple[str, str]]:
    """(mode, prompt) ciftlerinden olusan karisik bir liste dondurur."""
    rng = random.Random(seed)
    bank: list[tuple[str, str]] = []

    for topic, detail in itertools.product(SITE_TOPICS, SITE_DETAILS):
        bank.append(("site", f"{topic.capitalize()} icin {detail}.{rng.choice(STYLE_HINTS)}"))
    for task in APP_TASKS:
        for _ in range(4):
            bank.append(("app", f"{task.capitalize()}.{rng.choice(STYLE_HINTS)}"))
    for task in CODE_TASKS:
        for _ in range(3):
            bank.append(("code", f"{task}. Kullanim ornegi ve README ekle."))

    rng.shuffle(bank)
    return bank


# --------------------------------------------------------------------------
# Sunucu ile konusma
# --------------------------------------------------------------------------


def post_json(url: str, payload: dict, timeout: int) -> dict:
    request = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={"content-type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def stream_generation(server: str, project_id: str, prompt: str, timeout: int) -> tuple[int, str]:
    """Uretim akisini sonuna kadar okur. (dosya_sayisi, hata) dondurur."""
    request = urllib.request.Request(
        f"{server}/api/projects/{project_id}/generate",
        data=json.dumps({"prompt": prompt}).encode("utf-8"),
        headers={"content-type": "application/json"},
        method="POST",
    )
    files, error = 0, ""
    with urllib.request.urlopen(request, timeout=timeout) as response:
        event = ""
        for raw in response:
            line = raw.decode("utf-8", "replace").rstrip("\n")
            if line.startswith("event:"):
                event = line[6:].strip()
            elif line.startswith("data:"):
                try:
                    data = json.loads(line[5:].strip())
                except json.JSONDecodeError:
                    continue
                if event == "done":
                    files = len(data.get("files", []))
                elif event == "error":
                    error = data.get("message", "bilinmeyen hata")
    return files, error


def delete_project(server: str, project_id: str) -> None:
    request = urllib.request.Request(f"{server}/api/projects/{project_id}", method="DELETE")
    try:
        urllib.request.urlopen(request, timeout=30).close()
    except urllib.error.URLError:
        pass


def main() -> None:
    parser = argparse.ArgumentParser(description="OPT-MUS ogretmen modelden veri damitma")
    parser.add_argument("--server", default="http://localhost:3000")
    parser.add_argument("--count", type=int, default=100, help="uretilecek ornek sayisi")
    parser.add_argument("--concurrency", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--keep-projects", action="store_true",
                        help="uretilen projeleri workspace icinde birak")
    args = parser.parse_args()

    server = args.server.rstrip("/")
    try:
        with urllib.request.urlopen(f"{server}/api/status", timeout=15) as response:
            status = json.loads(response.read().decode("utf-8"))
    except urllib.error.URLError as error:
        raise SystemExit(f"Sunucuya ulasilamadi ({server}): {error}\nOnce `npm start` calistirin.")

    if not status.get("ready"):
        raise SystemExit(f"Saglayici hazir degil: {status.get('hint')}")
    if status.get("provider") == "mock":
        raise SystemExit("Saglayici 'mock'. Damitma icin gercek bir ogretmen model gerekir.")

    print(f"Ogretmen: {status['provider']} · {status['model']}")

    bank = build_prompt_bank(args.seed)
    tasks = [bank[i % len(bank)] for i in range(args.count)]
    queue: Queue = Queue()
    for index, task in enumerate(tasks, 1):
        queue.put((index, task))

    lock = threading.Lock()
    totals = {"ok": 0, "fail": 0, "files": 0}

    def worker() -> None:
        while not queue.empty():
            try:
                index, (mode, prompt) = queue.get_nowait()
            except Exception:
                return
            try:
                project = post_json(f"{server}/api/projects",
                                    {"prompt": prompt, "mode": mode}, args.timeout)["project"]
                files, error = stream_generation(server, project["id"], prompt, args.timeout)
                if not args.keep_projects:
                    delete_project(server, project["id"])

                with lock:
                    if error or files == 0:
                        totals["fail"] += 1
                        print(f"[{index}/{len(tasks)}] HATA  {mode}: {error or 'dosya uretilmedi'}")
                    else:
                        totals["ok"] += 1
                        totals["files"] += files
                        print(f"[{index}/{len(tasks)}] tamam {mode}: {files} dosya · {prompt[:60]}")
                    sys.stdout.flush()
            except Exception as error:  # noqa: BLE001 - tek ornek hatasi tum isi durdurmasin
                with lock:
                    totals["fail"] += 1
                    print(f"[{index}/{len(tasks)}] HATA  {error}")
            finally:
                queue.task_done()

    threads = [threading.Thread(target=worker, daemon=True) for _ in range(max(1, args.concurrency))]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    print(f"\nBitti. Basarili: {totals['ok']}  Hatali: {totals['fail']}  "
          f"Toplam dosya: {totals['files']}")
    print("Sonraki adim: python3 training/build_dataset.py")


if __name__ == "__main__":
    main()
