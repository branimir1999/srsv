Program funkcionira tako da postoje dvije dretve:
Glavna dretva koja je odgovorna za pozivanje prekida svakih 100 ms te dretva obrade koja obrađuje
zadatak. Glavna dretva spava 100 milisekundi pa napravi novu dretvu obrade koja pokreće svoj
program, a glavna dretva počne spavati nakon toga. Svaki prekid je jedan period rada (u ovom
slučaju period od 100 ms) gdje dretva obrade može obraditi svoj zadatak.

Dretva obrade prvo provjerava je li zadatak gotov. U slučaju da nije, provjerava zadovoljava li
uvjet za produljenje rada trenutnog zadatka. Ako da, novonastala dretva završava i vraća se na
prethodni zadatak. U suprotnom, bira se novi zadatak tako da se kod izvršava dalje. Dretva 
postavlja lokalne varijable potrebne za obrađivanje zadatka. Prvo, uzima se novi zadatak iz
slijeda zadataka te postavlja se jedinstven broj tom zadatku. Dalje, izračunava se vrijeme obrade
zadatka tako da se nasumično odabere 30, 50, 80 ili 120 ms trajanje obrade. Nakon toga, u petlji
obavlja se količina rada od 5 ms dok se ne obavi sav posao (ili ne dogodi novi prekid). Pri
završetku ili prekidu, provjeravamo je li bio prekid ili završetak zadatka. Ako je bio završetak,
resetiraju se globalne varijable i ovisno je li bilo produljenja, povećava streak ili započinje
novi zadatak. Ako je bio prekid, onda se izlazi iz funkcije i čeka sljedeći prekid glavne dretve. 

Pomoćne funkcije su:
- msleep() koji radi spavanje u zadanim milisekundama, 
- chooseTask() koji po redoslijedu odabire sljedeći zadatak
- taskProcessTime() koji nasumično odabire trajanje obrade zadatka 

Vrijedno je spomenuti posebne slučajeve. U većini slučajeva, program će raditi kao očekivano.
Jedina situacija kada će se dogoditi prekid trenutnog nedovršenog posla je u 5% slučajeva kada
je trajanje posla dulje nego perioda novog prekida. Ako se to dogodi, onda sljedeće ponašanje
ovisi o trajanju sljedećeg posla ukoliko zadovoljava uvjete produljenja:
- ako je 30 ili 50 ms, onda će program raditi kao očekivano
- za 80 ms, ponašanje nije determinističko definirano, ali najvjerojatnije će biti donji slučaj
- za 120 ms, budući da je istrošeno produljenje (streak je na 0), taj novi posao od 120 ms se
trajno prekida.
Ako ne zadovoljava uvjete produljenja, onda će se samo pokrenuti novi posao.

U ovoj vježbi simuliran je sustav s jednostavnim periodičkim poslovima s +2 dretve. Jedna glavna, 
a ostale nastaju pri periodičkom (100 ms) prekidu. Obrađuje se točno jedan zadatak svaki period 
između dva prekida. Ako zadatak nije gotov, postoji mogućnost produljenja za najviše jednu periodu,
ali novi zadatak  se mora započeti pri završetku trentunog. Osnovna skica programa prekida: 
obradi_zadatak {
	x = odaberi zadatak za obradu;
	obradi zadatak x
}
Redoslijed zadataka je statički definiran u funkciji chooseTask(). Redoslijed sadrži 80 zadataka
koji se izvode kroz 8 sekundi pa onda redoslijed kreće iz početka.

Prekidi se ne izvode mehanički već programatski. Svaki novi prekid je nova dretva, te ukoliko
postoji prekoračenje vremena, postojat će u tom trenutku dvije radne dretve. Međutim, nova
radna dretva će postaviti globalne varijable processing_task_id/posao_u_obradi tako da će
onemogućiti daljni rad stare dretve preko uvjeta u "ako" naredbama.

Postoje simulacije ulaza kao u prvoj laboratorijskoj vježbi, ali u reduciranoj formi. Ulaz
je beskonačna petlja gdje se u svakoj periodi generira novi ulaz. Nema interakcije s ostatkom
programa.

Trajanje obrade je simulirano dinamički preko vjerojatnosti kao u prvoj laboratorijskoj vježbi, 
ali vrijednosti za vjerojatnosti su definirane statički prema ovoj distribuciji:
- 30ms:  20%
- 50ms:  50%
- 80ms:  25%
- 120ms: 5%

Grupiranje je izvedeno na sljedeći način:
- Grupa zadataka A ima periodu 1 s i ima zadatke s jednoznamenkastim indeksima:
{0, 1, 2, 3, 4}
- Grupa zadataka B ima periodu 2 s i ima zadatke s dvoznamenkastim indeksima:
{10, 11, 12, 13, 14, 15}
- Grupa zadataka C ima periodu 4 s i ima zadatke s troznamenkastim indeksima:
{100, 101, 102, 103};
- Grupa zadataka D ima periodu 8 s i ima zadatke s četveroznamenkastim indeksima:
{1000, 1001, 1002}
gdje je '1002' indeks za prazan zadatak grupe D.

Pri završetku simulacije, statistika se ispisuje sa sljedećim podacima: 
- broj obavljenih poslova
- broj zadaka koji su koristili dvije periode
- broj zadataka koji se nisu obavili do kraja
- prosječno vrijeme obrade po zadatku
Ove statistike su grupirane po kategorijama zadataka (A,B,C,D) te globalno (sve kategorije zajedno).




