from multiprocessing import Pool
import sys
import os
import time

oecc = [0, 1] # 0 : SEC-DED (Hsiao), 1 : SSC-DSD (RS-code)
fault = [0, 1, 2, 3, 4, 5, 6] # Bit=0, Pin=1, Byte=2, Double_Bit=3, Triple_Bit=4, Beat=5, Entry=6

for oecc_param in oecc:
    for fault_param in fault:
        os.system("./Fault_sim_start {0:d} {1:d} &".format(oecc_param, fault_param))