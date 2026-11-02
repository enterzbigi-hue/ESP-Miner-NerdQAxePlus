# Kompilacja na GitHubie — NerdQAxe++ Auto Tuner

1. Rozpakuj `koparka-auto-tuner.zip` i wgraj zawartość katalogu projektu do swojego repozytorium GitHub. W katalogu głównym repozytorium muszą być `CMakeLists.txt`, `main/`, `components/`, `test/`, `docs/` oraz `.github/`. Nie wgrywaj ZIP-a jako jedynego pliku.
2. Plik `build-nerdqaxe-autotuner.yml` musi znajdować się w `.github/workflows/` na domyślnej gałęzi repozytorium. Paczka źródłowa już go tam zawiera.
3. Otwórz **Actions → NerdQAxe++ Auto Tuner - BIN → Run workflow**. Jeżeli GitHub pyta o włączenie Actions, włącz je dla tego repozytorium.
4. Po zielonym zakończeniu otwórz wykonanie, przewiń do **Artifacts** i pobierz **nerdqaxe-auto-tuner-bin**. Pliki są dostępne przez 14 dni; kolejne uruchomienie tworzy nowy zestaw.
5. Rozpakuj pobrany ZIP. W panelu koparki wgraj najpierw `esp-miner-NerdQAxe++.bin`, poczekaj na restart, następnie `www.bin`. Szczegóły: `WGRYWANIE.txt`.

Workflow wykonuje testy, buduje panel i firmware dla **NERDQAXEPLUS2 / ESP32-S3**, używając ESP-IDF 5.3.6 oraz Node.js 20. Sprawdza rozmiary partycji i limit ZIP-a poniżej 20 MB. Sumy SHA256 i identyfikator commita są dołączone do plików.

Uruchomienie jest ręczne. Nie wymaga własnych sekretów GitHuba. Nie publikuje wydania ani nie łączy się z koparką. To generator plików do aktualizacji OTA; sam plik YAML nie zastępuje źródeł projektu.

Workflow przygotowano i sprawdzono lokalnie. Nie uruchomiono go jeszcze na koncie GitHub użytkownika. Lokalna kompilacja odpowiadającego mu projektu przeszła; test na fizycznej koparce pozostaje do wykonania.
