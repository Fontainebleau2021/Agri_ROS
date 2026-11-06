import argparse
from pathlib import Path
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from seedgerm_common import ROOT
p=argparse.ArgumentParser();p.add_argument('--dir',default=str(ROOT/'_run_traits_baseline'));a=p.parse_args()
o=Path(a.dir);s=pd.read_csv(o/'image_trait_summary.csv')
for col in ['seed_area_sum','seedling_area_sum','seedling_skeleton_length_sum','overall_area_in_roi_sum']:
 plt.figure(figsize=(7,4));plt.plot(s.hours_from_start,s[col],marker='o');plt.xlabel('Hours from start');plt.ylabel(col);plt.tight_layout();plt.savefig(o/f'{col}.png',dpi=180);plt.close()
plt.figure(figsize=(7,4))
for i in range(1,5):plt.plot(s.hours_from_start,s[f'phase_{i}_count'],marker='o',label=f'phase {i}')
plt.xlabel('Hours from start');plt.ylabel('Detected boxes');plt.legend();plt.tight_layout();plt.savefig(o/'phase_counts.png',dpi=180);plt.close()
print('plots saved:',o)
