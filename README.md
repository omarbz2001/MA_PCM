# Projet TSP Parallèle - Documentation

## 1. Introduction

Ce projet a pour objectif de résoudre le **Problème du Voyageur de Commerce (TSP)** en utilisant une approche parallèle avec l'algorithme **branch-and-bound**. L'implémentation utilise des threads C++ et une structure de données **non-bloquante** pour distribuer le travail entre les cœurs disponibles.

### Objectifs Principaux
- Implémenter une solution parallèle du TSP avec branch-and-bound  
- Utiliser une structure de données partagée non-bloquante  
- Mesurer le **speed-up** et l'**efficacité** sur machine multi-cœurs  
- Fonctionner sur **Xeon Phi** (jusqu'à 256 threads)

## 2. Architecture du Système

### 2.1 Structure Séquentielle Originale
Le code de base fourni comprend :
- Algorithme branch-and-bound pour explorer l’arbre des permutations
- Modèle de tâches avec méthodes `split()`, `merge()`, `solve()`
- Graph TSP avec matrice de distances pré-calculée
- Chemins partiels représentant l’état de la recherche

### 2.2 Stratégie de Parallélisation

#### Décomposition du Problème
Le problème TSP se prête naturellement au parallélisme de données :
- Arbre de recherche : chaque branche peut être explorée indépendamment
- Tâches : chaque nœud de l’arbre représente une tâche traitée par un thread
- Bornes partagées : la meilleure solution trouvée sert à élaguer les branches inutiles

#### Modèle Producteur-Consommateur

```ascii
┌─────────────┐     ┌──────────────────┐     ┌─────────────┐
│   Tâches    │────▶│ Pool de Tâches   │────▶│   Threads   │
│  Générées   │     │  Lock-Free       │     │ Travailleurs│
└─────────────┘     └──────────────────┘     └─────────────┘
                           │                         │
                           ▼                         ▼
                    ┌──────────────────┐     ┌─────────────┐
                    │  Borne Globale   │◀─── │ Résultats  │
                    │ (Meilleure Sol.)  │    │  Partiels   │
                    └──────────────────┘     └─────────────┘
```

### 2.3 Composants Clés

#### Structure de Données Non-Bloquante
- **Implémentation** : Pile (LIFO) lock-free avec opérations atomiques  
- **Avantage** : Évite les blocages, meilleure scalabilité  
- **Problème ABA** : Résolu avec des pointeurs taggués  

#### Pool de Threads
- **Taille fixe** : Paramétrable à l’exécution  
- **Équilibrage de charge** : Vol de travail implicite via pile partagée  
- **Détection de terminaison** : Basée sur compteurs atomiques  

#### Gestion de la Borne (Bound)
- **Variable atomique** : Borne globale accessible à tous les threads  
- **Mise à jour** : Compare-and-swap (CAS) pour éviter les races  
- **Élagage** : Les threads vérifient périodiquement la borne  

## 3.Algorithme Parallèle
### 3.1 Processus d'Exécution

1. **Initialisation**
   - Chargement du graphe TSP  
   - Création du pool de threads  
   - Initialisation de la borne à l’infini
2. **Exploration Parallèle**
```ascii 
Pour chaque thread :
  Tant que non terminé :
    1. Prendre une tâche de la pile
    2. Si tâche vide → vérifier terminaison
    3. Split : générer sous-tâches (branches)
    4. Solve : explorer récursivement
    5. Mise à jour de la borne si meilleure solution
```
3. **Terminaison** 
    - Toutes les tâches traitées
    - Tous les threads inactifs
    - Meilleure solution retournée

### 3.2 Optimisations Implémentées
#### Élagage Précoce
- Vérification de la borne avant expansion
- Élimination des branches non-prometteuses
- Réduction exponentielle de l'espace de recherche

#### Granularité des Tâches
- Paramètre **cutoff** contrôle la profondeur de split
- Équilibre entre parallélisme et surcharge
- Adaptation selon le nombre de threads

#### Mémoire Locale
- Chemins partiels stockés par thread
- Réduction des allocations dynamiques
- Cache-friendly pour performances

## 4.Usage de programme
### 4.1 Compilation
```
# Compilation standard
make all

# Nettoyage
make clean

# Compilation avec débogage
make debug
```
### 4.2 Exécution
```
./parallel_tsp <fichier.tsp> <nombre_villes> <nombre_threads>
```

