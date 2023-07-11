# Reliability

# HBM2E ECC block configuration
- Data: 256 bit
- System ECC redundancy: 32 bit
- On-Die ECC redundancy: 24 bit
- Num of DQ: 64 (Psuedo-channel mode)
- Num of ECC-DQ: 8
- Burst Length: 4

# Prior work (Fig. 1)
![An Overview of the prior work](https://github.com/xyz123479/ITC-CSCC_23-EPA_ECC/blob/master/1_Reliability/EPA%20ECC_Prior%20work_SEC_DED.png)
- OD-ECC: [104, 96] SEC-DED X 3

# Motivation (Table 1) - Dominant soft error patterns
![Motivation](https://github.com/xyz123479/ITC-CSCC_23-EPA_ECC/blob/master/1_Reliability/EPA%20ECC_Soft%20error%20pattern.png)

# Code Layout of EPA ECC (Fig. 3)
![An overview of the EPA ECC](https://github.com/xyz123479/ITC-CSCC_23-EPA_ECC/blob/master/1_Reliability/EPA%20ECC_Prior%20work_SSC_DSD.png)
- OD-ECC: [39, 36] SSC-DSD over GF(256)
- Using Reed-Solomon code **[1]**

# References
- **[1]** Reed, Irving S., and Gustave Solomon. "Polynomial codes over certain finite fields." Journal of the society for industrial and applied mathematics 8.2 (1960): 300-304.
