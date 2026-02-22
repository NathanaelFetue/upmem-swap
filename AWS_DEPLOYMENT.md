# AWS/GCP Quick Deployment Guide

## ⚡ 5-Minute Setup for Testing

### Step 1: Launch EC2 Instance (AWS)

```bash
# Option A: AWS Console
- AMI: Ubuntu 22.04 LTS (free tier eligible)
- Instance type: t3.medium (2 vCPUs, 4 GB RAM)
- Storage: 10 GB EBS gp2
- Security group: Allow SSH (port 22)

# Option B: AWS CLI
aws ec2 run-instances \
  --image-id ami-0c55b159cbfafe1f0 \
  --instance-type t3.medium \
  --key-name your-key-pair \
  --security-groups default
```

### Step 2: SSH Into Instance

```bash
# Get public IP from console
ssh -i your-key.pem ubuntu@<PUBLIC_IP>
```

### Step 3: Install Build Tools (30 seconds)

```bash
sudo apt update
sudo apt install -y git gcc make cmake build-essential python3-pip python3-matplotlib
```

### Step 4: Clone & Build (2 minutes)

```bash
cd ~
git clone https://github.com/Pegasus04-Nathanael/upmem-swap.git
cd upmem-swap/simulator

# Build simulator
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Test
./swap_sim --dpus=8 --ram-mb=16 --accesses=10000 --working-set=1000
```

### Step 5: Run Benchmark (3 minutes)

```bash
cd ~/upmem-swap/simulator

# Quick benchmark
./swap_sim --dpus=8 --ram-mb=32 --accesses=100000 --working-set=10000 --batch-size=50

# Save results
./swap_sim --dpus=8 --ram-mb=32 --accesses=100000 --working-set=10000 --batch-size=50 \
  --output=results/benchmark_aws.csv
```

### Step 6: Download Results (optional)

```bash
# On your local machine
scp -i your-key.pem ubuntu@<PUBLIC_IP>:~/upmem-swap/simulator/results/*.csv .
```

---

## GCP Compute Engine Alternative

```bash
# Launch VM
gcloud compute instances create upmem-test \
  --image-family=ubuntu-2204-lts \
  --image-project=ubuntu-os-cloud \
  --machine-type=e2-medium \
  --zone=us-central1-a

# SSH
gcloud compute ssh upmem-test --zone=us-central1-a

# Same steps as AWS from Step 3 onwards
```

---

## Azure VM Alternative

```bash
# Create VM
az vm create \
  --resource-group myResourceGroup \
  --name upmem-vm \
  --image UbuntuLTS \
  --size Standard_B2s \
  --admin-username azureuser \
  --generate-ssh-keys

# SSH
ssh azureuser@<IP>

# Same steps as AWS from Step 3 onwards
```

---

## Expected Output (4KB page, 1 DPU, parallel mode)

```
Swap-out latency: ~31 µs
Swap-in latency: ~39 µs
Throughput: ~32 pages/ms

Peak throughput (8-16 DPU): 450-600 MB/s
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `gcc: command not found` | `sudo apt install gcc` |
| `cmake: command not found` | `sudo apt install cmake` |
| Out of memory | Reduce `--ram-mb` or `--accesses` |
| Permission denied | Make sure you have correct SSH key |
| Compilation error | Check if you have 2GB+ free space |

---

## Cost Estimate (as of Feb 2026)

| Cloud | Instance | Cost/Month (free tier) |
|-------|----------|------------------------|
| AWS | t3.medium | $0 (12 months free) |
| GCP | e2-medium | $0 (free tier) |
| Azure | B2s | $0 (12 months free) |

---

**Total time to deploy + test: ~10 minutes**
