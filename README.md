# QuizzGame (Categoria B) [Propunere Continental]

## Descriere Proiect

Implementați un server multithreading care suportă oricâți clienți. Serverul va coordona clienții care răspund la un set de întrebări pe rând, în ordinea în care s-au înregistrat. Fiecărui client i se pune o întrebare și are un număr `n` de secunde pentru a răspunde la întrebare. Serverul verifică răspunsul dat de client și, dacă este corect, va reține punctajul pentru acel client. De asemenea, serverul sincronizează toți clienții între ei și oferă fiecăruia un timp de `n` secunde pentru a răspunde.

Comunicarea între server și client se va realiza folosind socket-uri. Toată logica va fi realizată în server, clientul doar răspunde la întrebări. Întrebările cu variantele de răspuns vor fi stocate fie în fișiere XML, fie într-o bază de date SQLite. Serverul va gestiona situațiile în care unul din participanți părăsește jocul astfel încât jocul să continue fără probleme.

## Tehnologii Utilizate

- **Limbaj de programare**: C++ 
- **Bază de date**: SQLite (pentru stocarea întrebărilor și variantelor de răspuns)
- **Socket-uri**: Pentru comunicarea între server și clienți

## Funcționalități Cheie

1. **Înregistrarea Clienților**: Clienții se pot înregistra și se vor alinia pentru a răspunde la întrebări în ordinea înregistrării.
2. **Întrebări și Răspunsuri**: Fiecărui client i se oferă o întrebare și un timp limitat pentru a răspunde.
3. **Verificarea Răspunsurilor**: Serverul verifică răspunsurile și acordă puncte pentru cele corecte.
4. **Sincronizare**: Serverul sincronizează timpul de răspuns pentru toți clienții.
5. **Gestionarea Deconectărilor**: Serverul gestionează deconectările astfel încât jocul să continue fără probleme.

