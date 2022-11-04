Kod je strukturiran ovako:

Na početku programa su definirane konstante i globalne varijable. Postoje dvije posebne strukture, a to su:
"input_data_structure" i "message". Struktura "input_data_structure" sadrži sve početne varijable potrebne za
pokretanje dretve simulatora ulaza (id, početna odgoda te perioda). Struktura "message" predstavlja abstrakciju
komuniciranja između dretvi simulatora ulaza i upravljačke dretve. Dakle, sadrži sve informacije koje bi se
inače razmijenile normalnim metodama, ali ovdje smo zbog jednostavnosti iskoristili globalne varijable po kojem
svaka dretva ima jednu instancu strukture koju dijeli s upravljačkom dretvom.

Dodan je custom signal gdje se ispisuje statistika dretvi nakon što se pritisne CTRL + C te prekida rad ostalih
dretvi. Dodane su dvije zasebne funkcije čekanja, msleep() i sleep() koje su u milisekundama i sekundama. Također,
dodane su dvije vjerojatnosne funkcije processing() i random_period() koje nasumično odabiru vrijeme čekanja za
upravljačku dretvu i dretvu simulatora ulaza po proizvoljnoj vjerojatnosnoj distribuciji. Konačno od pomoćnih
funkcija, imamo randomFloat() koji samo vraća realni broj iz zadanog intervala.

Sljedeće imamo upravljačku dretvu koja zapisuje prethodna stanja i postavlja ih na default vrijednost od -1.
Nakon toga, ulazi u beskonačnu petlju (tj do prekida rada programa) gdje provjerava svaku dretvu po redu je li
se stanje dotične dretve promijenilo. Ukoliko se stanje promijenilo, postavlja trenutno stanje kao prethodno
stanje, vraća dretvi simulatora ulaza odgovor da je ulaz obrađen i vrijeme (timestamp) kad je ulaz obrađen.

Dretva simulatora ulaza prvo postavlja svoj zadani id i pomoćne varijable. Nakon toga spava određeno vrijeme
iz strukture "input_data_structure". Tada ulazi u beskonačnu petlju (opet do prekida rada programa) i generira
nasumičnu vrijednost ulaza iz intervala [100, 1000]. Dohvaća vrijeme kada se to dogodilo i čeka svoju periodu
i dodatnu odgodu ukoliko je ima. Ako nakon periode nije odgovor obrađen, onda zapisuje da je odgovor kasnio
i preispitava svaku milisekundu je li odgovor stigao. Ako je odgovor stigao, nasumično generira dodatnu odgodu
te nadodaje ju na periodu.

Konačno, glavna dretva main() postavlja i stvara ostale dretve te čeka da upravljačka dretva završi prije nego
ona sama završi sa radom.
