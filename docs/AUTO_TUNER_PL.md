# Auto tuner extranonce2

## Działanie

- Tuner jest domyślnie wyłączony. W tym trybie punkt startowy określa pole **Start po wyłączeniu (%)** (0–100%, domyślnie 73,24%). Procent jest liczony od wybranego licznika; Auto oznacza cały przydział puli. Kolejne zadania zmniejszają wartość; licznik nie zawija się do góry. Dla 4 bajtów pierwszy punkt to `BB7E90FE`.
- Nowa zakładka **Auto tuner** jest dostępna w bocznym menu panelu, pod adresem `/#/pages/tuner`.
- Pole **Początek nowego skanu (%)** przyjmuje 0,01–100%, z dokładnością do dwóch miejsc po przecinku; można użyć przecinka lub kropki. Dotyczy nowego skanu. Start po wyłączeniu jest ustawiany osobnym polem obok przycisku wyłączenia.
- **Rozpocznij nowy pomiar** usuwa poprzednie wyniki i uruchamia skan od podanego procentu.
- Domyślny skan obejmuje 20 przedziałów: 100–95%, 95–90%, …, 5–0%. Wewnątrz każdego przedziału extranonce2 maleje o 1 na nowe zadanie.
- Przy własnym początku, np. 73,24%, przedziały to 73,24–68,24%, 68,24–63,24%, …, 3,24–0%. Ostatni może być węższy niż 5 punktów procentowych.
- Po 10 zaobserwowanych wzrostach wysokości bloku sieci Bitcoin tuner przechodzi do następnego przedziału. Wysokość jest odczytywana z coinbase, a nie z liczby udziałów ani komunikatów puli. Ten sam blok z dwóch pul lub wiele zadań dla jednej wysokości nie zwiększa licznika. Pierwsza znana wysokość jest punktem odniesienia. Skok wysokości po przerwie w połączeniu zalicza jedną obserwację, aby nie kończyć zakresów bez pomiarów.
- Po zakończeniu skanu wybierany jest zakres z najwyższym zarejestrowanym diffem. Program pozostaje w nim i nadal zbiera wyniki. Opóźnione odpowiedzi ASIC trafiają do pierwotnego zakresu; mogą jeszcze poprawić końcowy wybór. Przy remisie wygrywa pierwszy zakres w kolejności skanu.
- Jeśli podczas całego skanu nie zarejestrowano żadnego poprawnego trafienia ASIC, tuner wyłącza się i wraca do zapisanego startu ręcznego.
- **Wyłącz i zastosuj start** zatrzymuje eksperyment i rozpoczyna zwykłą sekwencję od procentu wpisanego obok. **Zastosuj start** zmienia tę wartość również wtedy, gdy tuner jest już wyłączony. **Wznów skan** wraca do zachowanego postępu.

## Wyniki i trwałość

Panel pokazuje aktualnie wysłane extranonce2, zakres, postęp, liczbę bloków, czas, liczbę zadań i poprawnych próbek ASIC. Każdy zakres ma pięć najwyższych diffów wraz z dokładnymi wartościami extranonce2. Wyniki można wyeksportować do CSV.

Diffy są zapisywane jako liczby `double`, bez obcinania do 32 bitów. Wynik jest przypisany do eksperymentu i zakresu w chwili utworzenia zadania. Wyniki starego eksperymentu nie zanieczyszczają nowego pomiaru.

Stan eksperymentu, liczniki i ranking są zapisywane w osobnej przestrzeni NVS co 60 sekund oraz przy zmianie trybu i zaobserwowanej zmianie bloku. Po restarcie eksperyment jest odtwarzany. Nagły zanik zasilania może utracić ostatnią minutę zmian. Błąd zapisu jest widoczny w panelu.

## Obsługiwany zakres

- Lokalne układy ASIC urządzenia.
- Stratum V1 i V2 Extended: wybór licznika 4, 8, 16, 32, 64, 128 lub 256 bitów oraz Auto (pełny przydział puli). Domyślnie 32 bity = 4 bajty = 8 znaków HEX = `FFFFFFFF`. Mniejszy licznik jest dopełniany zerami do długości wymaganej przez pulę. Większy od przydziału nie uruchamia pomiaru; zwykłe kopanie trwa dalej.
- Stratum V2 Standard jest dostępny jako rzeczywisty tryb połączenia i pozwala kopać, ale nie udostępnia extranonce2 do regulacji. W zakładce tunera rozwiń sekcję połączenia puli, wybierz V1, V2 Standard albo V2 Extended i zapisz. Zmiana typu kanału ponownie łączy pulę. Oddzielny filtr sesji określa, które połączenia tuner uwzględnia w pomiarze; nie zmienia protokołu puli.
- Urządzenia podrzędne CAN zachowują dotychczasowy sposób tworzenia extranonce2 i nie są uwzględniane w eksperymencie.
- Po wyczerpaniu wartości w zakresie licznik nie zawija się w górę. Urządzenie czeka na przejście do następnego zakresu; przy pracy w wybranym zakresie potrzebny będzie wtedy nowy pomiar lub wyłączenie tunera.
- Zmiany ustawień korzystają z istniejącej ochrony sieciowej i mechanizmu OTP projektu, jeśli OTP jest włączone.

## Weryfikacja

Przeszły:

1. Produkcyjna kompilacja Angular (`npm run build`), wraz z kompresją zasobów.
2. Natywne testy rzeczywistego modułu tunera z kontrolowanym zegarem i pamięcią NVS, z AddressSanitizer oraz UndefinedBehaviorSanitizer.
3. Porównanie 17 280 przedziałów z niezależnymi obliczeniami całkowitoliczbowymi Pythona; dokładne 73,24% dla każdej szerokości od 1 do 256 bitów. Testy długości HEX, niezgodnej sesji, migracji zapisanych wyników i przełączania kanału Standard/Extended.
4. Testy budowy coinbase i danych extranonce2 dla rzeczywistych klas V1 oraz V2 Extended, z zastąpionym transportem i funkcjami skrótu.
5. Istniejące kontrole statyczne CORS/OTP i CAN.
6. Kontrola zakładki w przeglądarce na danych demonstracyjnych. Zrzut `podglad-auto-tunera.png` nie przedstawia pomiarów z koparki.

**Pełna kompilacja firmware’u ESP32-S3 dla NerdQAxe++ zakończyła się pomyślnie 16.10.2026.** Użyto ESP-IDF 5.3.6 i wariantu `NERDQAXEPLUS2`. Wersja aplikacji: `autotuner-20261016-manual`. Zapis NVS wykonuje osobne zadanie ze stosem w wewnętrznej pamięci RAM, aby uniknąć zapisu flash z zadania ze stosem PSRAM. Obraz aplikacji ma 3 413 136 bajtów i mieści się w partycji 4 MiB (19% wolnego miejsca). Esptool potwierdził poprawną sumę kontrolną oraz hash obrazu.

Gotowe pliki są w osobnej paczce `nerdqaxe-auto-tuner-bin.zip`: `esp-miner-NerdQAxe++.bin` oraz `www.bin`. W tej paczce znajduje się instrukcja ręcznego wgrania i sumy SHA256. Należy zaktualizować oba pliki.

Do powtórzenia testów logiki wystarczą Python 3 i kompilator C++17:

```sh
python3 test/run_auto_tuner_native.py
```

Do ponownego zbudowania firmware’u należy użyć ESP-IDF 5.3.6, `BOARD=NERDQAXEPLUS2` i instrukcji projektu. Plik `dependencies.lock` przypina komponenty mDNS i LVGL. Biblioteka secp256k1 jest dołączona w wersji wskazanej przez upstream: `0cdc758a56360bf58a851fe91085a327ec97685a`.

## GitHub Actions

Plik `.github/workflows/build-nerdqaxe-autotuner.yml` buduje oba pliki BIN i sprawdza limit ZIP-a poniżej 20 MB. Instrukcja uruchomienia: `docs/GITHUB_AUTO_TUNER_PL.md`. Zapisano go także osobno w plikach wynikowych. Składnia YAML, skrypty Bash i etap pakowania zostały sprawdzone lokalnie. Nie uruchomiono jeszcze tego workflow na GitHubie.

## Interpretacja eksperymentu

Ranking opisuje dotychczasowe trafienia. Nie jest modelem przewidującym przyszłe wyniki. Hash proof-of-work ma charakter pozornie losowy, więc historycznie najwyższy diff w danym zakresie nie oznacza większej szansy na kolejne wysokie trafienie. Zobacz [Bitcoin Developer Guide](https://developer.bitcoin.org/devguide/block_chain.html).
