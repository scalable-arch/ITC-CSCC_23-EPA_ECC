# Reliability evaluation

# Prior work (Fig. 1)
![An Overview of the prior work](https://github.com/xyz123479/ITC-CSCC_23-EPA_ECC/blob/master/1_Reliability/EPA%20ECC_Prior%20work_SEC_DED.png)
- OD-ECC: [104, 96] SEC-DED X 3

# Motivation (Table 1) - Dominant soft error patterns
![Motivation](https://github.com/xyz123479/ITC-CSCC_23-EPA_ECC/blob/master/1_Reliability/EPA%20ECC_Soft%20error%20pattern.png)

# Code Layout of EPA ECC (Fig. 3)
![An overview of the EPA ECC](https://github.com/xyz123479/ITC-CSCC_23-EPA_ECC/blob/master/1_Reliability/EPA%20ECC_Prior%20work_SSC_DSD.png)
- OD-ECC: [39, 36] SSC-DSD over GF(256)
- Using Reed-Solomon code **[2]**

# Code flows (Fault_sim.cpp)
- 1. Setting ECC-block configuration & error scenarios.
- 2. Setting output function name: output.S file.
- 3. **(Start loop)** HBM2E ECC-block setup
- 4. Initialize all data in ECC-block to 0
- 5. Error injection: Errors occur based on the error scenarios. **(Caution!) This evaluation has no fault!**
- 6. Apply **OD-ECC (On-Die ECC)**
>> Prior work: Apply the Hsiao SEC-DED code of (104, 96) to each ECC block.
>> EPA-ECC: Apply the RS SSC-DSD code of [39, 36] to an ECC block.
- 7. Report CE/DUE/SDC results.
- 8. **(End loop)** Derive final results.

# HBM2E ECC block configuration [1]
- Data: 256 bit
- System ECC redundancy: 32 bit
- On-Die ECC redundancy: 24 bit
- Num of DQ: 64 (Psuedo-channel mode)
- Num of Redundancy-DQ: 8
- Burst Length: 4

# References
- **[1]** Chun, Ki Chul, et al. "A 16-GB 640-GB/s HBM2E DRAM with a data-bus window extension technique and a synergetic on-die ECC scheme." IEEE Journal of Solid-State Circuits 56.1 (2020): 199-211.
- **[2]** Reed, Irving S., and Gustave Solomon. "Polynomial codes over certain finite fields." Journal of the society for industrial and applied mathematics 8.2 (1960): 300-304.
