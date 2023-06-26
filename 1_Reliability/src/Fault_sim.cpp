#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctime>
#include<iostream>
#include<cstdlib>
#include <vector>
#include <set>
#include <algorithm>
#include <math.h>
#include <cstring>

// Configuration 
// 필요로 하면 변경하기
#define CHANNEL_WIDTH 72 // channel 길이 (data DQ수 + ECC DQ 수) [64 + 8]
#define DATA_LEN 64 // channel마다 data 길이 (data DQ 수)
#define BLHEIGHT 4 // Burst length 길이
#define SYMBOL_SIZE 8 // SSC-DSD에서 실행하는 symbol size (8이면 GF(2^8))

#define OECC_CW_LEN 312 // ondie ecc의 codeword 길이 (bit 단위)
#define OECC_DATA_LEN 288 // ondie ecc의 dataward 길이 (bit 단위)
#define OECC_REDUN_LEN 24 // ondie ecc의 redundancy 길이 (bit 단위)


#define RUN_NUM 100000000 // 실행 횟수


//configuration over

using namespace std;
// GF(2^8)의 primitive element table 생성
unsigned int primitive_poly[16][256]={0,}; // 16가지 primitive polynomial 각각 256개 unique 한 값들 (각 row의 맨 끝에는 0을 나타낸다.) ex : primitive_poly[4][254] = a^254, primitive_poly[4][255] = 0 (prim_num=4인 경우이고, primitive_poly = x^8+x^6+x^4+x^3+x^2+x^1+1)
unsigned int H_Matrix_SEC_DED[OECC_REDUN_LEN/3][OECC_CW_LEN/3]; // 8 x 104

enum OECC_TYPE {SEC_DED=0, SSC_DSD=1}; // oecc_type
enum FAULT_TYPE {Bit=0, Pin=1, Byte=2, Double_Bit=3, Triple_Bit=4, Beat=5, Entry=6}; // fault_type
enum RESULT_TYPE {NE=0, CE=1, DUE=2, SDC=3}; // result_type

// 지정한 정수에서, 몇번째 비트만 읽어서 반환하는 함수
int getAbit(unsigned short x, int n) { 
  return (x & (1 << n)) >> n;
}

// 다항식 형태를 10진수로 변환
unsigned int conversion_to_int_format(char *str_read, int m)
{
    unsigned int primitive_value=0;
    if(strstr(str_read,"^7")!=NULL)
        primitive_value+=int(pow(2,7));
    if(strstr(str_read,"^6")!=NULL)
        primitive_value+=int(pow(2,6));
    if(strstr(str_read,"^5")!=NULL)
        primitive_value+=int(pow(2,5));
    if(strstr(str_read,"^4")!=NULL)
        primitive_value+=int(pow(2,4));
    if(strstr(str_read,"^3")!=NULL)
        primitive_value+=int(pow(2,3));
    if(strstr(str_read,"^2")!=NULL)
        primitive_value+=int(pow(2,2));
    if(strstr(str_read,"^1+")!=NULL) // 무조건 다음에 +1은 붙기 때문!
        primitive_value+=int(pow(2,1));
    if(strstr(str_read,"+1")!=NULL)
        primitive_value+=int(pow(2,0));
    

    return primitive_value;
}

// primitive polynomial table 생성
void generate_primitive_poly(unsigned int prim_value, int m, int prim_num)
{
    unsigned int value = 0x1; // start value (0000 0001)
    int total_count = int(pow(2,m));
    int count=0;
    while (count<total_count-1){ // count : 0~254
        primitive_poly[prim_num][count]=value;
        if(value>=0x80){ // m번째 숫자가 1이면 primitive polynomial과 xor 연산
            // value의 m+1번째 숫자를 0으로 바꾸고 shift
            value=value<<(32-m+1);
            value=value>>(32-m);

            //primitive polynomial과 xor 연산
            value=value^prim_value;
        }
        else // m+1번째 숫자가 0이면 왼쪽으로 1칸 shift
            value=value<<1;
        
        count++;
    }

    return;
}

// OECC, RECC, FAULT TYPE 각각의 type을 string으로 지정. 이것을 기준으로 뒤에서 error injection, oecc, recc에서 어떻게 할지 바뀐다!!!
void oecc_recc_fault_type_assignment(string &OECC, string &FAULT, int *oecc_type, int *fault_type, int oecc, int fault)
{
    // 1. OECC TYPE 지정
    // int oecc, int fault, int recc는 main함수 매개변수 argv로 받은 것이다. run.py에서 변경 가능
    switch (oecc){
        case SEC_DED:
            OECC.replace(OECC.begin(), OECC.end(),"HSIAO");
            *oecc_type=SEC_DED;
            break;
        case SSC_DSD:
            OECC.replace(OECC.begin(), OECC.end(),"RS");
            *oecc_type=SSC_DSD;
            break;
        default:
            break;
    }
    switch (fault){
        case Bit:
            FAULT.replace(FAULT.begin(), FAULT.end(),"Bit");
            *fault_type=Bit;
            break;
        case Pin:
            FAULT.replace(FAULT.begin(), FAULT.end(),"Pin");
            *fault_type=Pin;
            break;
        case Byte:
            FAULT.replace(FAULT.begin(), FAULT.end(),"Byte");
            *fault_type=Byte;
            break;
        case Double_Bit:
            FAULT.replace(FAULT.begin(), FAULT.end(),"Double_Bit");
            *fault_type=Double_Bit;
            break;
        case Triple_Bit:
            FAULT.replace(FAULT.begin(), FAULT.end(),"Triple_Bit");
            *fault_type=Triple_Bit;
            break;
        case Beat:
            FAULT.replace(FAULT.begin(), FAULT.end(),"Beat");
            *fault_type=Beat;
            break;
        case Entry:
            FAULT.replace(FAULT.begin(), FAULT.end(),"Entry");
            *fault_type=Entry;
            break;
        default:
            break;
    }
    return;
}

// 1 bit error (1 bit Error injection)
void error_injection_BIT(unsigned int OECC_array[])
{
    /*
    1 bit error
     -> 전체 On-die ECC 312b 중에서 임의의 1bit error
    */
    int Fault_pos;
    Fault_pos = rand()%OECC_CW_LEN; // 0~311
    OECC_array[Fault_pos]^=1;
    return;
}

// 1 pin error (1 pin Error injection)
void error_injection_PIN(unsigned int OECC_array[])
{
    /*
    pin error
    -> DQ0 : 0, 78, 156, 234
    -> DQ1 : 1, 79, 157, 235
    -> DQ2 : 2, 80, 158, 236
    ...
    -> DQ 71 : 71, 149, 227, 305
    -> 나머지 24BIT는 X (ODECC redundancy 부분이기에 pin이 없다.)
    */
    
    int Fault_pos=rand()%CHANNEL_WIDTH; // 0~71
    vector <int> Fault_pos_vector;

    // error 발생할 위치 고르기
    while(1){
        // 초기화
        Fault_pos_vector.clear();

        // Fault pos부터 72씩 더해서 뽑기
        for(int error_count=0; error_count<BLHEIGHT; error_count++){ // 0~3
            if(rand()%2)
                Fault_pos_vector.push_back(Fault_pos+error_count*(OECC_CW_LEN/BLHEIGHT)); // (0, 78, 156, 234) 'or' (1, 79, 157, 235), ...
        }

        // 최소 2개 이상의 bit error가 발생했는지 검사
        // 만약, 0~1개의 bit error가 발생했으면 다시 돌리기
        if(Fault_pos_vector.size()>=2)
            break;
    }

    // error injection
    for (int index=0; index<Fault_pos_vector.size(); index++)
        OECC_array[Fault_pos_vector[index]]^=1; // 해당 위치에서 bit-flip 발생
    
    return;
}

// 1 byte error (1 byte Error injection)
void error_injection_BYTE(unsigned int OECC_array[])
{
    /*
    byte error
    -> 0~7 범위, 8~15 범위, ..., 304~311 범위
    -> 즉 시작은 0, 8, 16, ... 304 이다.
    -> 8(symbol size)로 나누면 0, 1, 2, ... 38 이다.
    */

    int Fault_pos=rand()%(OECC_CW_LEN/SYMBOL_SIZE); // 0~38
    vector <int> Fault_pos_vector;

    // error 발생할 위치 고르기
    while(1){
        // 초기화
        Fault_pos_vector.clear();

        // Fault pos * 8(symbol size) 부터 1씩 더해서 뽑기
        for(int error_count=0; error_count<SYMBOL_SIZE; error_count++){ // 0~7
            if(rand()%2)
                Fault_pos_vector.push_back(Fault_pos*SYMBOL_SIZE+error_count); // 0~7 또는 8~15 또는 .... 304~311
        }

        // 최소 2개 이상의 bit error가 발생했는지 검사
        // 만약, 0~1개의 bit error가 발생했으면 다시 돌리기
        if(Fault_pos_vector.size()>=2)
            break;
    }

    for (int index=0; index<Fault_pos_vector.size(); index++)
        OECC_array[Fault_pos_vector[index]]^=1; // 해당 위치에서 bit-flip 발생

    return;
}

// 2 bit error (2 bit error injection)
void error_injection_DOUBLE_BIT(unsigned int OECC_array[])
{
    /*
    2 bit error
    -> 전체 312b OECC block 중에서 임의의 2bit error 발생
    -> 이때, 같은 pin이나 같은 symbol에는 포함되지 않는다.

    */

    // error 발생할 위치 고르기
    int First_fault_pos;
    int Second_fault_pos;
    while(1){
        First_fault_pos=rand()%OECC_CW_LEN; // 0~311
        Second_fault_pos=rand()%OECC_CW_LEN; // 0~311
        
        // 2개의 error 위치가 같은 경우면 다시 뽑기
        if(First_fault_pos==Second_fault_pos)
            continue;
        // 2개의 error 위치가 같은 pin에 속하면 다시 뽑기
        // 1. mod 78하면 0~71이 나와야 한다.
        // 2. 그리고, 서로 차이가 78의 배수어야 한다.
        if((First_fault_pos%(OECC_CW_LEN/BLHEIGHT)<CHANNEL_WIDTH) && (Second_fault_pos%(OECC_CW_LEN/BLHEIGHT))<CHANNEL_WIDTH){
            if(abs(First_fault_pos-Second_fault_pos)%(OECC_CW_LEN/BLHEIGHT)==0)
                continue;
        }
        // 2개의 error 위치가 같은 byte에 속하면 다시 뽑기
        // 8로 나눈 몫이 같으면 된다.
        if(First_fault_pos/SYMBOL_SIZE == Second_fault_pos/SYMBOL_SIZE)
            continue;

        // 위 중 어느 조건에도 해당되지 않으면 루프 탈출!
        break;
    }

    // error injection
    OECC_array[First_fault_pos]^=1; // 해당 위치에서 bit-flip 발생
    OECC_array[Second_fault_pos]^=1; // 해당 위치에서 bit-flip 발생
    return;
}

// 3 bit error (3 bit error injection)
void error_injection_TRIPLE_BIT(unsigned int OECC_array[])
{
    /*
    3 bit error
    -> 전체 312b OECC block 중에서 임의의 3bit error 발생
    -> 이때, 같은 pin이나 같은 symbol에는 포함되지 않는다.

    */

    // error 발생할 위치 고르기
    int First_fault_pos;
    int Second_fault_pos;
    int Third_fault_pos;
    while(1){
        First_fault_pos=rand()%OECC_CW_LEN; // 0~311
        Second_fault_pos=rand()%OECC_CW_LEN; // 0~311
        Third_fault_pos=rand()%OECC_CW_LEN; // 0~311
        
        // 3개의 error 위치가 하나라도 경우면 다시 뽑기
        if(First_fault_pos==Second_fault_pos || First_fault_pos==Third_fault_pos || Second_fault_pos==Third_fault_pos)
            continue;
        // 3개의 error 위치가 같은 pin에 속하면 다시 뽑기
        if((First_fault_pos%(OECC_CW_LEN/BLHEIGHT)<CHANNEL_WIDTH) && (Second_fault_pos%(OECC_CW_LEN/BLHEIGHT))<CHANNEL_WIDTH && (Third_fault_pos%(OECC_CW_LEN/BLHEIGHT))<CHANNEL_WIDTH){
            if(abs(First_fault_pos-Second_fault_pos)%(OECC_CW_LEN/BLHEIGHT)==0 && abs(First_fault_pos-Third_fault_pos)%(OECC_CW_LEN/BLHEIGHT)==0 && abs(Second_fault_pos-Third_fault_pos)%(OECC_CW_LEN/BLHEIGHT)==0)
                continue;
        }
        // 3개의 error 위치가 같은 byte에 속하면 다시 뽑기
        if((First_fault_pos/SYMBOL_SIZE == Second_fault_pos/SYMBOL_SIZE) && (Second_fault_pos/SYMBOL_SIZE == Third_fault_pos/SYMBOL_SIZE))
            continue;

        // 위 중 어느 조건에도 해당되지 않으면 루프 탈출!
        break;
    }

    // error injection
    OECC_array[First_fault_pos]^=1; // 해당 위치에서 bit-flip 발생
    OECC_array[Second_fault_pos]^=1; // 해당 위치에서 bit-flip 발생
    OECC_array[Third_fault_pos]^=1; // 해당 위치에서 bit-flip 발생d
    return;
}

// 1 beat error (1 beat error injection)
void error_injection_BEAT(unsigned int OECC_array[])
{
    /*
    1 beat error
    -> 4개의 beat 중에서 1개의 beat에서 error 발생. (최소 4개 bit 이상)
    -> 이때, 해당 error들은 같은 byte에 속하지 않아야 한다. (어차피 같은 pin에 속하는 경우는 없다.)
    -> 근데 어차피 확률상 beat마다 64개의 bit이 있는데, 각각 50% 확률로 bit-flip 된다고 해도 1 byte에 속하는 경우나 error가 4bit 미만인 경우는 거의 0%에 수렴한다.
    -> 결론 : 즉, 그냥 50% bit-flip으로 돌려도 문제가 없다.

    */

    int Fault_beat_pos=rand()%BLHEIGHT; // 0~3 (error가 발생하는 beat 위치)
    int error_index=Fault_beat_pos*(OECC_CW_LEN/BLHEIGHT);
    int count=0;
    while(count<(DATA_LEN)){ // 0~63
        if(rand()%2)
            OECC_array[error_index+count]^=1; // 0~77, 78~155, 156~233, 234~311
        count++;
    }

    return;
}

// 1 entry error (1 entry error injection)
void error_injection_ENTRY(unsigned int OECC_array[])
{
    /*
    1 entry error
    -> 4개의 beat 중에서 최소 2개 이상의 beat에서 error 발생. (합해서 최소 4개 bit 이상)
    -> 근데 어차피 확률상 beat마다 78개의 bit이 있는데, 각각 50% 확률로 bit-flip 된다고 해도 error가 4bit 미만인 경우는 거의 0%에 수렴한다.
    -> 결론 : 즉, 그냥 50% bit-flip으로 돌려도 문제가 없다.

    */

    int error_beat_num=rand()%(BLHEIGHT-1)+2; // 0~2 -> 2~4 => error가 발생한 beat 수
    int beat_2_error_case=rand()%6; // (0,1), (0,2), (0,3), (1,2), (1,3), (2,3) [4C2]
    int beat_3_error_case=rand()%4; // (0,1,2), (0,1,3), (0,2,3), (1,2,3) [4C3]
    //printf("error_beat_num : %d\n",error_beat_num);
    switch (error_beat_num){
        case 2: // (0,1), (0,2), (0,3), (1,2), (1,3), (2,3) [4C2]
            {
                switch(beat_2_error_case){
                    case 0:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count]^=1; // 0~63 (Beat 0)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)]^=1; // 78~155 (Beat 1)
                                count++;
                            }
                        }
                        break;
                    case 1:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count]^=1; // 0~63 (Beat 0)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*2]^=1; // 156~233 (Beat 2)
                                count++;
                            }
                        }
                        break;
                    case 2:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count]^=1; // 0~63 (Beat 0)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*3]^=1; // 234~311 (Beat 3)
                                count++;
                            }
                        }
                        break;
                    case 3:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)]^=1; // 78~155 (Beat 1)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*2]^=1; // 156~233 (Beat 2)
                                count++;
                            }
                        }
                        break;
                    case 4:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)]^=1; // 78~155 (Beat 1)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*3]^=1; // 234~311 (Beat 3)
                                count++;
                            }
                        }
                        break;
                    case 5:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*2]^=1; // 156~233 (Beat 2)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*3]^=1; // 234~311 (Beat 3)
                                count++;
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        case 3: // (0,1,2), (0,1,3), (0,2,3), (1,2,3) [4C3]
            {
                switch(beat_3_error_case){
                    case 0:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count]^=1; // (Beat 0)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)]^=1; //  (Beat 1)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*2]^=1; //  (Beat 2)
                                count++;
                            }
                        }
                        break;
                    case 1:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count]^=1; // (Beat 0)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)]^=1; //  (Beat 1)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*3]^=1; //  (Beat 3)
                                count++;
                            }
                        }
                        break;
                    case 2:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count]^=1; // (Beat 0)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*2]^=1; //  (Beat 2)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*3]^=1; //  (Beat 3)
                                count++;
                            }
                        }
                        break;
                    case 3:
                        {
                            int count=0;
                            while(count<(DATA_LEN)){ // 0~63
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)]^=1; // (Beat 1)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*2]^=1; //  (Beat 2)
                                if(rand()%2)
                                    OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*3]^=1; //  (Beat 3)
                                count++;
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        case 4: // (0,1,2,3) [4C4]
            {
                int count=0;
                while(count<(DATA_LEN)){ // 0~63
                    if(rand()%2)
                        OECC_array[count]^=1; // (Beat 0)
                    if(rand()%2)
                        OECC_array[count+(OECC_CW_LEN/BLHEIGHT)]^=1; // (Beat 1)
                    if(rand()%2)
                        OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*2]^=1; //  (Beat 2)
                    if(rand()%2)
                        OECC_array[count+(OECC_CW_LEN/BLHEIGHT)*3]^=1; //  (Beat 3)
                    count++;
                }
            }
            break;
        default:
            break;
    }

    return;
}

// OECC 1bit correction 2-bit error detection
int error_correction_oecc_SEC_DED(unsigned int OECC_array[], int ECC_start_position)
{
    /*
    OECC-SECDED는 다음과 같이 mapping 된다.

    OECC_0 : 0~71 (BEAT0) + 72~77 + 78~103 (BEAT1 일부) => 0~103
    OECC_1 : 104~149 (BEAT1 일부) + 150~155 + 156~207 (BEAT2 일부) => 104~207
    OECC_2 : 208~227 (BEAT2 일부) + 228~233 + 234~311 (BEAT3 일부) => 208~311


    error correction 및 
    */

    unsigned int Syndromes[OECC_REDUN_LEN/3]; // 8 x 1
    
    // Syndromes = H * C^T
    for(int row=0; row<(OECC_REDUN_LEN/3); row++){
        unsigned int row_value=0;
        for(int column=0; column<(OECC_CW_LEN/3); column++)
            row_value=row_value^(H_Matrix_SEC_DED[row][column] * OECC_array[ECC_start_position+column]);
        Syndromes[row]=row_value;
    }

    // Syndrome이 0인지 검사
    // Syndrome이 전부 0이면 NE(No-Error) return하고 종료
    int count=0;
    for(int index=0; index<(OECC_REDUN_LEN/3); index++){
        if(Syndromes[index]==1)
            count++;
    }
    if(count==0)
        return NE;

    // error correction (Check Syndromes)
    int cnt=0;
    for(int error_pos=0; error_pos<OECC_CW_LEN/3; error_pos++){
        cnt=0;
        for(int row=0; row<(OECC_REDUN_LEN/3); row++){
            if(Syndromes[row]==H_Matrix_SEC_DED[row][error_pos])
                cnt++;
            else
                break;
        }
        // 1-bit error 일때만 error correction 진행
        if(cnt==(OECC_REDUN_LEN/3)){
            OECC_array[ECC_start_position+error_pos]^=1; // error correction (bit-flip)
            return CE;
        }
    }

    // Error가 발생했지만 1-bit error는 아닌 경우이다.
    // 이 경우에는 correction을 진행하지 않는다.
    return DUE;
}

// OECC 1 symbol correction-2 symbol detection (8-bit symbol)
int error_correction_oecc_SSC_DSD(unsigned int OECC_array[])
{
    // primitive polynomial : x^8+x^4+x^3+x^2+1
    /*
        (1) Syndrome이 모두 0이면 => return NE
        (2) Syndrome이 0이 아니고, SSC의 syndrome과 일치하면 SSC 진행 [S1/S0이 a^0~a^38중 하나] => return CE
        (3) Syndrome이 0이 아니고, SSC의 syndrome과 일치하지 않으면 => return DUE

        H-Matrix는 아래와 같이 생겼다. (3 x 39 행렬 [symbol 기준])
        
        1 1 1 1 1 1 1.... 1
        1 a^1 a^2 a^3 ... a^38
        1 a^2 a^4 a^6 ... a^76
    */
    
    // codeword 생성
    unsigned int codeword[OECC_CW_LEN];
    for(int index=0; index<OECC_CW_LEN; index++)
        codeword[index]=OECC_array[index];

    //printf("\ncodeword : ");
    //for(int column=0; column<RECC_CW_LEN; column++)
    //    printf("%d ",codeword[column]);
    //printf("\n");

    // Syndrome 계산
    // codeword = (a^exponent0 , a^exponent1, a^exponent2, ... a^exponent38)

    // S0 = (a^exponent0) ^ (a^exponent1) ^ (a^exponent2) ... ^(a^exponent38)
    // S1 = (a^exponent0) ^ (a^[exponent1+1]) ^ (a^[exponent2+2]) ... ^ (a^[exponent38+38])
    // S2 = (a^exponent0) ^ (a^[exponent1+2]) ^ (a^[exponent2+4]) ... ^ (a^[exponent38+76])

    // Syndrome 계산
    unsigned int S0=0,S1=0,S2=0;
    for(int symbol_index=0; symbol_index<(OECC_CW_LEN/SYMBOL_SIZE); symbol_index++){ // 0~38
        unsigned exponent=255; // 0000_0000이면 255 (해당 사항만 예외케이스!)
        unsigned symbol_value=0; // 0000_0000 ~ 1111_1111
        // ex : codeword의 첫 8개 bit가 0 1 0 1 1 1 0 0 이면
        // symbol_value는 (0<<7) ^ (1<<6) ^ (0<<5) ^ (1<<4) ^ (1<<3) ^ (1<<2) ^ (0<<1) ^ (0<<0) = 0101_1100 이다.
        for(int symbol_value_index=0; symbol_value_index<SYMBOL_SIZE; symbol_value_index++){ // 8-bit symbol
            symbol_value^=(codeword[symbol_index*8+symbol_value_index] << (SYMBOL_SIZE-1-symbol_value_index)); // <<7, <<6, ... <<0
        }
        for(int prim_exponent=0; prim_exponent<255; prim_exponent++){
            if(symbol_value==primitive_poly[0][prim_exponent]){
                exponent=prim_exponent;
                break;
            }
        }
        //printf("symbol_index : %d, symbol_value : %d\n",symbol_index, symbol_value);

        if(exponent!=255){
            // S0 = (a^exponent0) ^ (a^exponent1) ^ (a^exponent2) ... ^(a^exponent38)
            // S1 = (a^exponent0) ^ (a^[exponent1+1]) ^ (a^[exponent2+2]) ... ^ (a^[exponent38+38])
            // S2 = (a^exponent0) ^ (a^[exponent1+2]) ^ (a^[exponent2+4]) ... ^ (a^[exponent38+76])
            S0^=primitive_poly[0][exponent];
            S1^=primitive_poly[0][(exponent+symbol_index)%255];
            S2^=primitive_poly[0][(exponent+symbol_index*2)%255];
        }
    }

    // S0 = a^p, S1= a^q, S2=a^r (a^0 ~ a^254) 또는 0000_0000
    unsigned int p,q,r;
    for(int prim_exponent=0; prim_exponent<255; prim_exponent++){
        if(S0==primitive_poly[0][prim_exponent])
            p=prim_exponent;
        if(S1==primitive_poly[0][prim_exponent])
            q=prim_exponent;
        if(S2==primitive_poly[0][prim_exponent])
            r=prim_exponent;
    }
    //printf("S0 : %d(a^%d), S1 : %d(a^%d)\n",S0,p,S1,q);

    //printf("S0 : %d\n",S0);
    if(S0==0 && S1==0 && S2==0){ // NE (No Error)
        return NE;
    }
    
    // CE 'or' DUE
    // error chip position

    // Correction 확인
    int error_symbol_location, error_check_location1, error_check_location2;
    error_symbol_location=(q+255-p)%255;
    error_check_location1=(r+255-q)%255; // 이것도 확인을 해야 DSD가 된다. (S0, S1, S2를 전부 확인해야 함)
    error_check_location2=(r+255-p)%255; // 이것도 확인을 해야 DSD가 된다. (S0, S1, S2를 전부 확인해야 함)

    /*
        SSC 진행하는 경우
        S0=a^p=a^n
        S1=a^q=a^(n+i)
        S2=a^r=a^(n+2i)

        (q+255-p)%255 = i => 0~38
        (r+255-q)%255 = i => 0~38
        ((r+255-p)%255) = 2i => 0~76
    */

    // Table
    // SSC 조건
    if(0<=error_symbol_location && error_symbol_location < (OECC_CW_LEN/SYMBOL_SIZE) && error_symbol_location==error_check_location1 && error_check_location2==2*error_symbol_location){ // CE (error symbol location : 0~38)
        // printf("CE case! error correction start!\n");
        //error correction
        for(int symbol_index=0; symbol_index<SYMBOL_SIZE; symbol_index++){ // 0~7
            OECC_array[error_symbol_location*SYMBOL_SIZE+symbol_index]^=getAbit(S0, SYMBOL_SIZE-1-symbol_index); // S0 >> 7, S0 >> 6 ... S0 >> 0
        }
        // printf("CE case! error correction done!\n");     
        return CE;
    }
    // Table End!!!!!
    
    // DUE
    // 신드롬이 0이 아니고, correction 진행 안한 경우
    return DUE;
}

// SDC (Silent Data Corruption) check
int SDC_check(unsigned int OECC_array[])
{
    // 1이 남아있는지 검사
    for(int error_pos=0; error_pos<OECC_CW_LEN; error_pos++){ // 0~311
        // 1이 남아 있다. => SDC (Silent Data Corruption) 발생
        if(OECC_array[error_pos]){
            return SDC;
        }
    }
    
    // 1이 없으면 error가 없다는 뜻이니 CE return
    return CE;
}

int main(int argc, char* argv[])
{
    // 1. GF(2^8) primitive polynomial table 생성
    // prim_num으로 구분한다!!!!!!!!!!!!!!!!!
    FILE *fp=fopen("GF_2^8__primitive_polynomial.txt","r");
    int primitive_count=0;
    while(1){
        char str_read[100];
        unsigned int primitive_value=0;
        fgets(str_read,100,fp);
        primitive_value=conversion_to_int_format(str_read, 8);

        generate_primitive_poly(primitive_value,8,primitive_count); // ex : primitive polynomial : a^16 = a^9+a^8+a^7+a^6+a^4+a^3+a^2+1 = 0000 0011 1101 1101 = 0x03DD (O) -> 맨 오른쪽 prim_num : 0
        primitive_count++;

        if(feof(fp))
            break;
    }
    fclose(fp);

    // 2. H_Matrix 설정
    // SEC-DED : OECC
    FILE *fp4=fopen("H_Matrix_SEC_DED.txt","r");
    while(1){
        unsigned int value;
        for(int row=0; row<OECC_REDUN_LEN/3; row++){
            for(int column=0; column<OECC_CW_LEN/3; column++){
                fscanf(fp4,"%d ",&value);
                H_Matrix_SEC_DED[row][column]=value;
                //printf("%d ",H_Matrix_binary[row][column]);
            }
        }
        if(feof(fp4))
            break;
    }
    fclose(fp4);


    // 3. 출력 파일 이름 설정 & oecc/fault/recc type 설정 (main 함수의 argv parameter로 받는다.
    // run.py에서 변경 가능!!!

    // 파일명 예시
    // ex : HSIAO_Beat.S -> OECC에는 HSIAO(SEC-DED), Error는 1beat error 발생
    // ex : RS_Double_Bit.S -> OECC는 RS(SSC-DSD), Error는 2bit error 발생
    string OECC="X", FAULT="X"; // => 파일 이름 생성을 위한 변수들. 그 이후로는 안쓰인다.
    int oecc_type, recc_type,fault_type; // => on-die ECC, Rank-level ECC, fault_type 분류를 위해 쓰이는 변수. 뒤에서도 계속 사용된다.
    oecc_recc_fault_type_assignment(OECC, FAULT, &oecc_type, &fault_type, atoi(argv[1]), atoi(argv[2]));
    
    string Result_file_name = OECC + "_" + FAULT + ".S";
    FILE *fp3=fopen(Result_file_name.c_str(),"w"); // c_str : string class에서 담고 있는 문자열을 c에서의 const char* 타입으로 변환하여 반환해주는 편리한 멤버함수

    // 4. 여기서부터 반복문 시작 (1억번)
    // On-die ECC 설정
    // HBM2E : On-die ECC에 대해서 (312, 288) 구성
    // 기존 SEC-DED : (104, 96) 3개로 구성
    // 본 논문 SSC-DSD : (312, 288) 1개로 구성 => [39, 36] 는 8bit symbol 기준

    unsigned int OECC_array[OECC_CW_LEN]; // On-die ECC block 전체 기준 : 312bit로 구성
    /*
        OECC block configuration (총 312 block)

        Beat 3 : 234 235 236 237 238 239 ..... 305 / 306 307 308 309 310 311
        Beat 2 : 156 157 158 159 160 161 ..... 227 / 228 229 230 231 232 233
        Beat 1 : 78  79  80  81  82  83 .....  149 / 150 151 152 153 154 155
        Beat 0 : 0   1   2   3   4   5 .....   71 /  72  73  74  75  76  77
    */

    int CE_cnt=0, DUE_cnt=0, SDC_cnt=0; // CE, DUE, SDC 횟수
    srand((unsigned int)time(NULL)); // 난수 시드값 계속 변화
    for(int runtime=0; runtime<RUN_NUM; runtime++){
        if(runtime%1000000==0){
            fprintf(fp3,"\n===============\n");
            fprintf(fp3,"Runtime : %d/%d\n",runtime,RUN_NUM);
            fprintf(fp3,"CE : %d\n",CE_cnt);
            fprintf(fp3,"DUE : %d\n",DUE_cnt);
            fprintf(fp3,"SDC : %d\n",SDC_cnt);
            fprintf(fp3,"\n===============\n");
	        fflush(fp3);
        }
        // 4-1. ODECC 312b 전부를 0으로 초기화 (no-error)
        // 이렇게 하면 굳이 encoding을 안해도 된다. no-error라면 syndrome이 0으로 나오기 때문!
        for(int i=0; i<OECC_CW_LEN; i++)
            memset(OECC_array, 0, sizeof(OECC_array)); 
        
        // 4-2. Error injection
        // [1] error injection (1 Bit, 1 Pin, 1 Byte, 2 Bit, 3 Bit, 1 Beat, 1 Entry)
        // 1 bit을 제외한 multi-bit error 경우에는 각 bit는 50% 확률로 bit-flip 발생


        switch (fault_type){
            case Bit: // 1bit
                error_injection_BIT(OECC_array);
                break; 
            case Pin: // 1pin
                error_injection_PIN(OECC_array);
                break;
            case Byte: // 1byte
                error_injection_BYTE(OECC_array);
                break;
            case Double_Bit: // 2bit
                error_injection_DOUBLE_BIT(OECC_array);
                break;
            case Triple_Bit: // 3 bit
                error_injection_TRIPLE_BIT(OECC_array);
                break;
            case Beat: // 1 beat
                error_injection_BEAT(OECC_array);
                break;
            case Entry: // 1 ENTRY
                error_injection_ENTRY(OECC_array);
                break;         
            default:
                break;
        }


        // 4-3. OECC
        /*
        OECC-SECDED (104, 96) 3개는 아래와 같이 mapping 된다.
            OECC_0 : 0~71 (BEAT0) + 72~77 + 78~103 (BEAT1 일부)
            OECC_1 : 104~149 (BEAT1 일부) + 150~155 + 156~207 (BEAT2 일부)
            OECC_2 : 208~227 (BEAT2 일부) + 228~233 + 234~311 (BEAT3 일부)
        
        OECC-SSCDSD [39, 36]은 0~7, 8~15, 16~23... 와 같이 symbol 단위로 mapping 된다.

        */
        // SEC-DED : 104개의 1-bit error syndrome에 대응하면 correction 진행. 아니면 안함 (mis-correction을 최대한 막아보기 위함이다.)
        //      -> DUE: 3개의 (104,96) SEC-DED에서 1개라도 DUE가 발생하는 경우
        //      -> CE : DUE가 없고, 1(error)이 남아있지 않은 경우
        //      -> SDC : 그 외 나머지 경우
        // SSC_DSD : 312b OECC block에 대해서 8-bit symbol [39, 36] SSC-DSD 실행한 겨웅
        //      -> DUE : DUE 결과 보고한 경우
        //      -> CE : DUE가 없고, 1(error)이 남아있지 않은 경우
        //      -> SDC : 그 외 나머지 경우
        int final_result;
        switch(oecc_type){
            case SEC_DED:
                {
                    int final_result1, final_result2, final_result3;
                    final_result1=error_correction_oecc_SEC_DED(OECC_array,0);
                    final_result2=error_correction_oecc_SEC_DED(OECC_array,104);
                    final_result3=error_correction_oecc_SEC_DED(OECC_array,208);
                    if(final_result1==DUE || final_result2==DUE || final_result3==DUE)
                        final_result=DUE;
                    else{
                        final_result=SDC_check(OECC_array);
                    }
                }
                break;
            case SSC_DSD: 
                {
                    final_result=error_correction_oecc_SSC_DSD(OECC_array);
                    if(final_result==CE || final_result==NE){
                        final_result=SDC_check(OECC_array);
                    }
                }
            default:
                break;
        }

        // 4-4. CE/DUE/SDC 체크
        // 최종 update
        // CE, DUE, SDC 개수 세기
        CE_cnt   += (final_result==CE)  ? 1 : 0;
        DUE_cnt  += (final_result==DUE) ? 1 : 0;
        SDC_cnt  += (final_result==SDC) ? 1 : 0;
            
    }
    // for문 끝!!

    // 최종 update
    fprintf(fp3,"\n===============\n");
    fprintf(fp3,"Runtime : %d\n",RUN_NUM);
    fprintf(fp3,"CE : %d\n",CE_cnt);
    fprintf(fp3,"DUE : %d\n",DUE_cnt);
    fprintf(fp3,"SDC : %d\n",SDC_cnt);
    fprintf(fp3,"\n===============\n");
    fflush(fp3);

    // 최종 update (소숫점 표현)
    fprintf(fp3,"\n===============\n");
    fprintf(fp3,"Runtime : %d\n",RUN_NUM);
    fprintf(fp3,"CE : %.11f\n",(double)CE_cnt/(double)RUN_NUM);
    fprintf(fp3,"DUE : %.11f\n",(double)DUE_cnt/(double)RUN_NUM);
    fprintf(fp3,"SDC : %.11f\n",(double)SDC_cnt/(double)RUN_NUM);
    fprintf(fp3,"\n===============\n");
    fflush(fp3);

    // 최종 update (백분율 표현)
    fprintf(fp3,"\n===============\n");
    fprintf(fp3,"Runtime : %d\n",RUN_NUM);
    fprintf(fp3,"CE : %.2f\n",((double)CE_cnt/(double)RUN_NUM)*100);
    fprintf(fp3,"DUE : %.2f\n",((double)DUE_cnt/(double)RUN_NUM)*100);
    fprintf(fp3,"SDC : %.2f\n",((double)SDC_cnt/(double)RUN_NUM)*100);
    fprintf(fp3,"\n===============\n");
    fflush(fp3);

    fclose(fp3);


    return 0;
}
