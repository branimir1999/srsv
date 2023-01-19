Prateći upute laboratorisjke vježbe, postoje dvije C datoteke: posluzitelj.c i generator.c.
Pri pokretanja obje datoteke, potrebno je postaviti okolinsku varijablu "SRSV_LAB4" i postaviti
željenu vrijednost. Za pokretanje posluzitelj.c, potrebno je dati dva argumenta - broj dretvi N
i granicu za zbrojeno trajanje svih trenutnih poslova u cekanju M. Za pokretanje generator.c,
potrebno je također navesti dva argumenta - broj poslova J i maksimalno trajanje posla K. Ako
ne damo dovoljan broj argumenata, javit će grešku i neće se pokrenuti.

Generator funkcionira tako da se može pokrenuti više generatora u paraleli. On je odgovoran za
stvaranje poslova tako da osigura jedinstvenost identifikatora, imena te prostor podataka. On
je također odgovoran za slanje poruka (odnosno zadataka u tekstovnom obliku) posluzitelju preko
reda poruka (zajednička memorija).

Posluzitelj ima dva tipa dretvi u svojem programu: zaprimatelj i radnik. Dretva zaprimatelj
stvara radne dretve te prima poruke (poslove) od generatora. Kad se zaprima dovoljno poslova
dretva budi radnike i radnici uzimaju poslove dok ih ima. Dretve radnici tipično čekaju dok
ne dobiju signal odnosno buđenje od zaprimatelja da je vrijeme raditi.