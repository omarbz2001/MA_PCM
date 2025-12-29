#!/usr/bin/env bash
set -euo pipefail

# --------- Valeurs par défaut ----------
DEFAULT_FILE="dj38.tsp"
DEFAULT_CITY_MIN=8
DEFAULT_CITY_MAX=15
DEFAULT_THREADS=(1 2 4 8 12 16 32 64 128 256)
DEFAULT_CUTOFFS=(0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)

BIN="./comparator_combined"

# --------- Vérifications ----------
if [[ ! -x "$BIN" ]]; then
  echo "Erreur: $BIN introuvable ou non exécutable"
  exit 1
fi

# --------- Entrées utilisateur ----------
read -p "Fichier TSP [${DEFAULT_FILE}]: " FILE
FILE=${FILE:-$DEFAULT_FILE}

read -p "Range nbr_villes (min max) [${DEFAULT_CITY_MIN} ${DEFAULT_CITY_MAX}]: " CITY_MIN CITY_MAX
CITY_MIN=${CITY_MIN:-$DEFAULT_CITY_MIN}
CITY_MAX=${CITY_MAX:-$DEFAULT_CITY_MAX}

read -p "Liste des threads (séparés par espace) [${DEFAULT_THREADS[*]}]: " -a THREADS
THREADS=("${THREADS[@]:-${DEFAULT_THREADS[@]}}")

read -p "Liste des cutoffs (séparés par espace) [${DEFAULT_CUTOFFS[*]}]: " -a CUTOFFS
CUTOFFS=("${CUTOFFS[@]:-${DEFAULT_CUTOFFS[@]}}")

# --------- Validation ----------
if (( CITY_MIN > CITY_MAX )); then
  echo "Erreur: CITY_MIN > CITY_MAX"
  exit 1
fi

echo
echo "Configuration:"
echo "  Fichier  : $FILE"
echo "  Villes   : $CITY_MIN -> $CITY_MAX"
echo "  Threads  : ${THREADS[*]}"
echo "  Cutoffs  : ${CUTOFFS[*]}"
echo

read -p "Confirmer l'exécution ? (y/n): " CONFIRM
[[ "$CONFIRM" == "y" ]] || exit 0

# --------- Exécution ----------
for (( city=CITY_MIN; city<=CITY_MAX; city++ )); do
  MAX_CUTOFF=$((city - 1))

  VALID_CUTOFFS=()
  for c in "${CUTOFFS[@]}"; do
    (( c <= MAX_CUTOFF )) && VALID_CUTOFFS+=("$c")
  done

  if [[ ${#VALID_CUTOFFS[@]} -eq 0 ]]; then
    echo "Skip nbr_villes=$city (aucun cutoff valide)"
    continue
  fi

  for t in "${THREADS[@]}"; do
    echo "Run: fichier=$FILE villes=$city threads=$t cutoffs=${VALID_CUTOFFS[*]}"
    "$BIN" "$FILE" "$city" "$t" "${VALID_CUTOFFS[@]}"
  done
done
