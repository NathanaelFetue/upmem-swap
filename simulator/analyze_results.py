#!/usr/bin/env python3
"""
UPMEM Swap Simulator - Results Analyzer
Lit les fichiers CSV et produit un résumé comparatif
"""

import csv
import os
import sys
from pathlib import Path

def read_csv(filename):
    """Lit un fichier CSV et retourne les données"""
    if not os.path.exists(filename):
        return None
    
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
    
    return rows[0] if rows else None

def format_float(value):
    """Formate un nombre avec 2 décimales"""
    try:
        return f"{float(value):.2f}"
    except:
        return str(value)

def main():
    results_dir = Path("results")
    
    if not results_dir.exists():
        print("❌ Aucun répertoire 'results' trouvé")
        return 1
    
    csv_files = list(results_dir.glob("*.csv"))
    
    if not csv_files:
        print("❌ Aucun fichier CSV trouvé dans 'results/'")
        return 1
    
    print("\n" + "=" * 100)
    print("UPMEM SWAP SIMULATOR - RESULTS ANALYSIS")
    print("=" * 100)
    
    # Groupe les résultats par catégorie
    scaling_tests = {}
    pattern_tests = {}
    other_tests = {}
    
    for csv_file in sorted(csv_files):
        data = read_csv(csv_file)
        if not data:
            continue
        
        filename = csv_file.stem
        
        # Catégorise autom atiquement
        if filename in ['1dpu', '4dpu', '8dpu', '16dpu']:
            scaling_tests[int(filename[0])] = (filename, data)
        elif data.get('pattern') in ['random', 'sequential', 'mixed']:
            pattern_tests[data.get('pattern')] = (filename, data)
        else:
            other_tests[filename] = data
    
    # Affiche les résultats de scaling
    if scaling_tests:
        print("\n📊 SCALING TESTS (DPU scalability)")
        print("-" * 100)
        print(f"{'DPUs':<8} {'Accesses':<12} {'Faults':<12} {'Hit Rate':<15} {'Swap Out (µs)':<18} {'Swap In (µs)':<18}")
        print("-" * 100)
        
        for dpu_count in sorted(scaling_tests.keys()):
            filename, data = scaling_tests[dpu_count]
            print(f"{data['nr_dpus']:<8} {data['total_accesses']:<12} {data['page_faults']:<12} "
                  f"{format_float(data.get('hit_rate', '0'))}%{' '*9} "
                  f"{format_float(data.get('avg_swapout_us', '0'))} µs{' '*7} "
                  f"{format_float(data.get('avg_swapin_us', '0'))} µs")
    
    # Affiche les résultats de patterns
    if pattern_tests:
        print("\n📈 WORKLOAD PATTERN TESTS")
        print("-" * 100)
        print(f"{'Pattern':<15} {'Accesses':<12} {'Faults':<12} {'Hit Rate':<15} {'Swap Out':<12} {'Swap In':<12}")
        print("-" * 100)
        
        patterns = ['random', 'sequential', 'mixed']
        for pattern in patterns:
            if pattern not in pattern_tests:
                continue
            filename, data = pattern_tests[pattern]
            print(f"{data['pattern']:<15} {data['total_accesses']:<12} {data['page_faults']:<12} "
                  f"{format_float(data.get('hit_rate', '0'))}%{' '*9} "
                  f"{data['swapouts']:<12} {data['swapins']:<12}")
    
    # Autres résultats
    if other_tests:
        print("\n📋 OTHER TESTS")
        print("-" * 100)
        for test_name, data in other_tests.items():
            print(f"\n{test_name}:")
            print(f"  RAM: {data.get('ram_mb')} MB | DPUs: {data.get('nr_dpus')} | "
                  f"Working Set: {data.get('working_set')} pages")
            print(f"  Hit Rate: {format_float(data.get('hit_rate', '0'))}%")
            print(f"  Swap Out: {format_float(data.get('avg_swapout_us', '0'))} µs | "
                  f"Swap In: {format_float(data.get('avg_swapin_us', '0'))} µs")
    
    print("\n" + "=" * 100)
    print("\n✅ Analysis complete!")
    print(f"📁 Found {len(csv_files)} result files in {results_dir}/")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
