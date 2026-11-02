# Pełny korzeń Merkle i nagłówek bloku

Firmware: `autotuner-20261018-headers`. Nowy odczyt `/api/v2/hash-capture` udostępnia nagłówki przygotowanych zadań i zwróconych trafień ASIC. Nie wymaga zmiany puli, portu ani panelu WWW. Zbieranie jest domyślnie wyłączone; kolektor włącza je tylko w RAM i wyłącza przy zwykłym zakończeniu. Restart urządzenia również je wyłącza.

Dane zapisują się na komputerze w plikach `PELNE-NAGLOWKI-YYYY-MM-DD.jsonl` w tym katalogu. Dzień w nazwie jest w UTC. Każdy wiersz jest osobnym obiektem JSON.

- `header_hex`: dokładnie 160 znaków HEX, czyli pełny 80-bajtowy nagłówek w serializacji Bitcoina.
- `merkle_root_internal_hex`: 32 bajty korzenia dokładnie w kolejności z nagłówka.
- `merkle_root_display_hex`: ten sam korzeń w odwróconej kolejności bajtów, do prezentacji.
- `type=job`: nagłówek bazowy przygotowanego zadania, z bazową wersją i początkowym nonce. ASIC zmienia nonce i bity wersji; ten rekord nie jest zgłoszonym trafieniem.
- `type=result`: nagłówek rzeczywistego wyniku zwróconego przez ASIC i spełniającego próg ASIC. Zawiera rzeczywiste nonce, wersję i `device_diff`. Nie wszystkie takie wyniki są wysyłane do puli ani akceptowane przez nią.
- `header_sha256d_display_hex`, `calculated_diff`, `verified`: hash i diff obliczone niezależnie przez komputer. Dla wyniku diff musi zgadzać się z obliczeniem urządzenia.
- `extranonce2_hex`, ID zadania puli, ID ASIC, `tuner_run`, `tuner_step`: kontekst skopiowany z tego samego zadania. ID ASIC może być używane wielokrotnie.
- `boot_id`, `sequence`, `device_uptime_us`: identyfikacja sesji, kolejności i czasu urządzenia. `received_at` to czas odbioru przez komputer.

To pełny nagłówek przygotowany przez koparkę z danych puli, a nie pełny blok ani zapis wszystkich miliardów prób nonce. Coinbase i transakcje nie są potrzebne do ponownego policzenia SHA256d zapisanego nagłówka, ale bez nich nie można niezależnie zweryfikować budowy całego drzewa Merkle.

Na koparce działa ograniczony bufor 32 rekordów RAM, bez zapisu na flash i bez dodatkowego haszowania dla telemetrii. Komputer odczytuje przyrost co 2 s. Przy przerwach sieci lub zatrzymaniu komputera bufor może się nadpisać; `buffer_gap` jawnie oznacza brakujące rekordy. Nie należy zakładać kompletności zbioru. Status kolektora i liczba błędów weryfikacji są zapisane lokalnie w `work/miner-check/header-collector-status.json`.

Kolektor kończy zbieranie po 24 godzinach od uruchomienia albo po zapisaniu 500 MB, zależnie od tego, co nastąpi wcześniej. Po wymuszonym zamknięciu komputera wyłączenie telemetrii na urządzeniu może się nie udać; wtedy pozostaje jedynie ograniczony bufor RAM.

## Weryfikacja na urządzeniu

Firmware wgrano na fizyczną koparkę. Potwierdzono zachowanie wszystkich dotychczasowych maksimów i liczników bloków tunera po restarcie. Koparka wznowiła przyjmowane przez pulę udziały przy 750 MHz i 1200 mV. Komputer odebrał 113 zweryfikowanych nagłówków, w tym 28 nagłówków trafień. W tym odczycie: 0 błędów weryfikacji i 0 brakujących rekordów. Każdy zapisany wynik sprawdzono SHA256d; diff zgodny z firmware.

Test lokalny: znany nagłówek i hash bloku genesis, kolejność bajtów, wykrywanie błędnego korzenia Merkle oraz bufor o stałej pojemności, odczyt przyrostowy i wykrywanie braków. Dotychczasowe testy tunera i builderów przeszły; pełna kompilacja ESP-IDF zakończona poprawnie.

Kolektor uruchomiono 18.10.2026 o 11:10 Europe/Warsaw. Limit czasu kończy się 19.10.2026 o 11:10 Europe/Warsaw; wcześniej może zadziałać limit 500 MB.
