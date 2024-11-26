Palermo: Improving the Performance of Oblivious Memory using Protocol-Hardware Co-Design
===================
This directory contains the artifact of the HPCA'25 paper Palermo: Improving the Performance of Oblivious Memory using Protocol-Hardware Co-Design. 

If you find this repo useful, please cite the following paper: 
```
@misc{ye2024palermoimprovingperformanceoblivious,
      title={Palermo: Improving the Performance of Oblivious Memory using Protocol-Hardware Co-Design}, 
      author={Haojie Ye and Yuchen Xia and Yuhan Chen and Kuan-Yu Chen and Yichao Yuan and Shuwen Deng and Baris Kasikci and Trevor Mudge and Nishil Talati},
      year={2024},
      eprint={2411.05400},
      archivePrefix={arXiv},
      primaryClass={cs.CR},
      url={https://arxiv.org/abs/2411.05400}, 
}
```

## Required hardware:
CPU with at least 1TB main memory

## Required packages:
g++ (9.4.0 preferred), cmake, python3 (Python 3.9 preferred), numpy, matplotlib

## **Step 0**: Generate Palermo main results (6 hours)
./run_main.sh

## **Step 1**: Generate Palermo software only results (2 hours)
git checkout palermo_sw && ./run_palermo_sw.sh

## **Step 2**: Generate RingORAM results (2 hours)
git checkout ringoram && ./run_ringoram.sh

## **Step 3**: Generate PrORAM results (6 hours)
git checkout proram && ./run_proram.sh

## **Step 4**: Generate other baselines (4 hours)
git checkout other_oram && ./run_otheroram.sh

## **Step 5**: Plot results in the paper (1 minute)
git checkout main && cd fig9 && python3 fig9.py

## **Step 6**: fig9/fig9.png should look the same as the paper Fig.9. 
