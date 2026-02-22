# GUIDE COMPLET AWS - Étape par Étape (Copy-Paste)

## ✅ Ce que tu as fait
✓ Instance EC2 créée (t3.medium, Ubuntu 22.04)

## 📋 Ce qu'il te reste

---

## ÉTAPE 1: Avoir la clé SSH

Quand tu as créé l'instance AWS, tu as téléchargé un fichier `.pem` (la clé).

```
Exemple: my-key-pair.pem
```

**Où il est?** Cherche dans `~/Downloads/` ou là où tu l'as mis.

---

## ÉTAPE 2: Donner les bonnes permissions à la clé

**IMPORTANT!** Ouvre un terminal sur TON ordinateur (pas SSH encore) et tape:

```bash
cd ~/Downloads
chmod 600 my-key-pair.pem
```

(Remplace `my-key-pair.pem` par le vrai nom de ton fichier)

---

## ÉTAPE 3: Trouver l'IP PUBLIC de ton instance

### Via AWS Console (le plus facile):
1. Va sur https://console.aws.amazon.com
2. Clique sur **EC2 Dashboard**
3. Clique sur **Instances**
4. Cherche ton instance (elle s'appelle comme tu l'as nommée)
5. Copie l'**IPv4 Public** (exemple: `54.123.45.67`)

**C'est ta PUBLIC_IP - tu en auras besoin!**

---

## ÉTAPE 4: Se connecter en SSH (depuis TON ordinateur)

Ouvre un terminal et tape:

```bash
ssh -i ~/Downloads/my-key-pair.pem ubuntu@PUBLIC_IP
```

**Remplace:**
- `my-key-pair.pem` → le vrai nom de ta clé
- `PUBLIC_IP` → l'IP que tu viens de copier (exemple: 54.123.45.67)

### Exemple réel:
```bash
ssh -i ~/Downloads/upmem-key.pem ubuntu@54.123.45.67
```

### Si ça demande "Are you sure?", tape:
```
yes
```

**✅ Si ça marche:** Tu es maintenant DANS la machine AWS!

---

## ÉTAPE 5: Vérifier que tu es dans la machine AWS

Une fois connecté, tu devrais voir quelque chose comme:

```
ubuntu@ip-172-31-XX-XX:~$
```

Des questions? **Oui** = continue. **Non** = reviens à l'étape 4.

---

## ÉTAPE 6: Installer les outils (30 secondes)

**COPIE-COLLE cette commande ENTIÈRE** dans le terminal AWS:

```bash
sudo apt update && sudo apt install -y git gcc make cmake build-essential python3-pip python3-matplotlib python3-numpy
```

**Attends que ça finisse** (tu verras `done` ou retour à `ubuntu@...`)

---

## ÉTAPE 7: Télécharger le code (Clone)

**COPIE-COLLE:**

```bash
cd ~ && git clone https://github.com/Pegasus04-Nathanael/upmem-swap.git
```

(Si tu dois utiliser un autre repo, remplace l'URL)

**Tu devrais voir** quelque chose comme:
```
Cloning into 'upmem-swap'...
remote: Enumerating objects...
```

---

## ÉTAPE 8: Construire le simulateur (Build)

**COPIE-COLLE:**

```bash
cd ~/upmem-swap/simulator && mkdir -p build && cd build && cmake .. && make -j$(nproc)
```

**Tu devrais voir:**
```
[ 10%] Building C object...
[ 20%] Linking...
[100%] Built target swap_sim
```

**Si ça dit "Built target swap_sim" à la fin = SUCCÈS!** ✓

---

## ÉTAPE 9: Test rapide (vérifier que ça marche)

**COPIE-COLLE:**

```bash
./swap_sim --dpus=1 --ram-mb=8 --accesses=1000
```

**Tu devrais voir** quelque chose comme:
```
Swap-out latency: 30.5 µs
Swap-in latency: 39.2 µs
Throughput: 32.7 pages/ms
```

**Si tu vois ça = TU ES BON!** ✅

---

## ÉTAPE 10: VRAI Benchmark (résultats pour l'article)

### Option A: Test rapide (30 secondes)
```bash
./swap_sim --dpus=8 --ram-mb=32 --accesses=50000 --working-set=5000 --batch-size=50
```

### Option B: Test complet (5 minutes)
```bash
./swap_sim --dpus=8 --ram-mb=32 --accesses=100000 --working-set=10000 --batch-size=50 --output=/tmp/benchmark_aws.csv
```

### Option C: Tests multiples (tous les configs)

**COPIE-COLLE TOUT D'UN COUP:**

```bash
cd ~/upmem-swap/simulator

# Test 1 DPU
./swap_sim --dpus=1 --ram-mb=16 --accesses=50000 --working-set=5000 --batch-size=50

# Test 8 DPU
./swap_sim --dpus=8 --ram-mb=32 --accesses=100000 --working-set=10000 --batch-size=50

# Test 16 DPU
./swap_sim --dpus=16 --ram-mb=64 --accesses=100000 --working-set=20000 --batch-size=50

# Test 64 DPU
./swap_sim --dpus=64 --ram-mb=128 --accesses=100000 --working-set=50000 --batch-size=50
```

**Ça va prendre ~5-10 minutes total.**

---

## ÉTAPE 11: Récupérer les résultats

### Option A: Résultats dans le terminal (Easy)

Si tu as vu les latences affichées dans le terminal à l'étape 10, **c'est bon!** Tu peux les copier directement.

### Option B: Télécharger un fichier CSV (Advanced)

D'abord, **enregistre les résultats**:

```bash
cd ~/upmem-swap/simulator
./swap_sim --dpus=8 --ram-mb=32 --accesses=100000 --working-set=10000 --batch-size=50 --output=results/benchmark.csv
```

Ensuite, **dans un NOUVEAU terminal sur ton ordinateur** (pas SSH):

```bash
cd ~/Downloads
scp -i ~/Downloads/my-key-pair.pem ubuntu@PUBLIC_IP:~/upmem-swap/simulator/results/benchmark.csv .
```

Tu auras alors un fichier `benchmark.csv` dans `~/Downloads/`

---

## ÉTAPE 12: Optionnel - Arrêter l'instance (pour économiser)

Quand tu en as fini:

**Via AWS Console:**
1. Va à EC2 → Instances
2. Clique sur ton instance
3. Clique "Instance State" → "Stop"

**Ça arrête l'instance** (tu ne paieras plus pendant qu'elle dort)

Pour la relancer: "Start instance"

---

## 🆘 Si quelque chose ne marche pas

### Erreur: "Permission denied (publickey)"
```
Cause: Mauvais chemin à la clé ou mauvaises permissions
Ceci:
  1. Vérifie le chemin: ls ~/Downloads/my-key-pair.pem
  2. Réapplique les permissions: chmod 600 ~/Downloads/my-key-pair.pem
  3. Réessaye SSH
```

### Erreur: "ssh: command not found"
```
Solution: Tu n'es pas sur macOS/Linux
→ Download PuTTY (Windows): https://www.putty.org/
```

### Erreur: "gcc: command not found" après apt install
```
Solution: Réessaye l'installation:
sudo apt update
sudo apt install -y build-essential
```

### Erreur: "CMake not found"
```
Solution:
sudo apt install cmake
```

### Erreur: Build échoue
```
Vérifie:
1. Free space: df -h
2. RAM libre: free -h
3. Si RAM < 1 GB: tue d'autres processus ou réduis --ram-mb
```

### Erreur: "No such file or directory"
```
Cause: La commande cd a échoué (tu n'es pas dans le bon dossier)
Fix:
1. pwd (affiche où tu es)
2. cd ~/upmem-swap
3. ls (vérifie qu'il y a des fichiers)
```

---

## 📊 Résumé Commandes Essentielles (Copy-Paste rapide)

```bash
# Sur TON ordinateur
chmod 600 ~/Downloads/my-key-pair.pem
ssh -i ~/Downloads/my-key-pair.pem ubuntu@PUBLIC_IP

# Maintenant tu es DANS AWS, continue:
sudo apt update && sudo apt install -y git gcc make cmake build-essential python3-pip python3-matplotlib python3-numpy
cd ~ && git clone https://github.com/Pegasus04-Nathanael/upmem-swap.git
cd ~/upmem-swap/simulator && mkdir -p build && cd build && cmake .. && make -j$(nproc)
./swap_sim --dpus=1 --ram-mb=8 --accesses=1000
./swap_sim --dpus=8 --ram-mb=32 --accesses=100000 --working-set=10000 --batch-size=50
```

---

## ✅ Checklist Avant de Commencer

- [ ] Fichier `.pem` trouvé (téléchargé depuis AWS)
- [ ] Public IP copié (depuis AWS Console)
- [ ] SSH fonctionne (tu vois `ubuntu@ip-...` dans le terminal)
- [ ] `apt update` terminé
- [ ] `cmake` compilé avec succès
- [ ] `./swap_sim --help` marche

**Si tout ✓ = Prêt à benchmarker!**

---

**Besoin d'aide?** Dis-moi le numéro de l'étape où tu bloques! 🚀
