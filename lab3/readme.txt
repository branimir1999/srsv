U ovoj vježbi, preuzet je kod iz prve laboratorijske vježbe. Naime, većina funkcija je sada izmijenjeno. Neke funkcije su obrisane. Međutim,
većina funkcija su potporne funkcije te glavne funkcije: glavna, petlja i upravljačka petlja, su malo izmijenjene.

Glavna petlja sada podržava SCHED_FIFO ovisno o zastavici, te ako je zastavica 0, onda se vraća na standardni način rada kao u laboratorijskoj
vježbi 1. Ako je zastavica 1, onda postavlja prioritete za nove dretve te stavlja SCHED_FIFO policy za dospijeće dretvi.

Upravljačka petlja je maknula 'for' petlju jer sada jedna upravljačka dretva obrađuje podatke točno jedne ulazne dretve. Dakle, modificirana
je da sluša svoju ulaznu dretvu te sada ima radno čekanje umjesto 'sleep'.

Konačno, dretva simulatora ulaza je ostala relativno nepromijenjena; samo su varijable promijenjene.

Nakon izvođenja inačice sa i bez LAB3RT zastavice približno 120 sekundi, rezultati su sljedeći:

Bez LAB3RT zastavice, svi imaju isti prioritet i možemo vidjet kašnjenje:
dretva sa id 0 ima udio zakašnjelih dretvi: 1%
dretva sa id 1 ima udio zakašnjelih dretvi: 0%
dretva sa id 2 ima udio zakašnjelih dretvi: 3%
dretva sa id 3 ima udio zakašnjelih dretvi: 4%
dretva sa id 4 ima udio zakašnjelih dretvi: 0%
dretva sa id 5 ima udio zakašnjelih dretvi: 0%
Globalno kašnjenje: 5/273 = 1.83%

Sa LAB3RT zastavice, možemo vidjet da (prioritet 1 je najveći prioritet):
dretva sa id 0 prioritetom (1) ima udio zakašnjelih dretvi: 0%
dretva sa id 1 prioritetom (2) ima udio zakašnjelih dretvi: 0%
dretva sa id 2 prioritetom (2) ima udio zakašnjelih dretvi: 1%
dretva sa id 3 prioritetom (3) ima udio zakašnjelih dretvi: 0%
dretva sa id 4 prioritetom (4) ima udio zakašnjelih dretvi: 0%
dretva sa id 5 prioritetom (5) ima udio zakašnjelih dretvi: 0%
Globalno kašnjenje = 1 / 275 = 0.36%


Vidimo da bez LAB3RT zastavice, svi imaju jednaki prioritet pa je raspodjela neučinkovita.
Zbog neučinkovite raspodjele, postoji veće globalno kašnjenje sustava. Sa LAB3RT zastavicom,
raspodjela prioriteta je učinkovitija jer je na osnovi duljine periode, gdje dretve koje se 
češće javljaju i kraće obrađuju imaju veći prioritet.

Također, jedino kašnjenje koje se dogodilo sa LAB3RT zastavicom je kod dretvi koje imaju
isti priroitet, a to su dreteve 1 i 2 koji međusobno koriste isti model (FIFO) kao kod
neučinkovitog modela bez LAB3RT zastavice.