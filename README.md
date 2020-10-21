# binaryclock
ajouter la gestion des alamres
->le fait de rajouter les capteurs capacitif fait osciller la lumière quand elle est faible ? a corriger? comprendre pourquoi
->ajouter des alarmes, les diode D1 et D2 permettent de voir quand elle sont actives
->propostion de fcontionement appuyi long sur rond permet de rentrer dans la configuration de lalarme 1, les feleche permettent de changer les heure appui court on passe sur les -> minutes appui court on passe sur on off.. comment afficher le passage dans les menus ?
ajouter le réglage de l'heure?
sortir du menu la croix?

Autre solution
appuis long haut activer/desactiver alarme 1
appuis long bas activer/desactiver alarme 2
appui long rond changer heure, appuis court rond changer minute apuis court alarme 1 appui court alarme 2 et croix pour sortir menu ??



A voir ajouter les interruptions provenant du RTC / cependant ce n'est pas sur une pin qui accepte les interruption, la 2 etant deja prise pour le pwm

a travailler
-> faire une machine a etat pour passer d'un etat a lautre
-> comment gerer les appui long et cour sans false trigger, genre pas transformer un long en deux courts.
-> comment gérer l'alarme , stocker lheure et la comparer avec Timlib



