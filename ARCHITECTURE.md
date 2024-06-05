# Architecture du Projet

Le projet s'articule autour d'un fichier `main.c` et d'un fichier `commands.c`. Il s'exécute avec `make run` ou en lancant les test avec `./test.sh`.

Le fichier `main.c` regroupe toutes les méthodes pour passer d'un type à l'autre pour pouvoir exécuter les commandes internes ou externes avec les bons arguments. Il est responsable aussi de l'affichage du `prompt` et de sa mise en forme avec l'ensemble des fonctions lié. Il utilise l'ensemble des fonctions du fichier `commands.c` afin de réaliser ses tâches. 

Le fichier `commands.c` contient toutes les fonctions en rapport avec les jobs, que ce soit pour les créer, les mettre à jour, envoyer des signaux, etc... Il contient la gestion des redirections et du passage à l'arrière plan. Il s'occupe entièrement des commandes externes également.

**Structure de donnée**
Le `jsh` `prompt` le dossier courant en le racourcissant en fonction de sa longueur avec le nombre de jobs en cours en début de ligne.
Lorsqu'une ligne de commande est entrée dans le `jsh`, elle est découpé et analyser par la fonction `parse_line()` qui s'occupe ensuite, en fonction de la nature de la commande, d'exécuter la commande en interne en vérifiant le nombre d'arguments ou de la transmettre à `externes_command()`. 
Cette méthode va alors créer le `job` correspondant à cette ligne et annalyser à son tour la ligne rentrée pour traiter les potentiels redirections et passage à l'arrière plan qui s'y trouve. 
Elle va ensuite exécuter la commande dans un fils et renvoyé au processus père in signal `SIGCHLD` qui va être interpretter pour mettre à jour le `job`. Une fois que la commande sera exécuter, le `jsh` qui boucle à l'infinie sur lui même tant qu'`exit` n'a pas été exécuter, reaffiche le `prompt` et attends la ligne de commande suivant.