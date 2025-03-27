# Relaxed Release Consistency for Acknowledgment-Free CPU-GPU Producer-Consumer Communication

## How to run
- Compile Murphi
    ```
    cd Cmurphi
    cd src && make
    ```
- Go to script directory.
    ```
    cd scripts
    ```
- Generate litmus test configs from config templates
    ```
    python3 genConfigFromBase.py
    ```
- Generate Murphi source file from source file templates and configs 
    ```
    python3 tempToMurphiSrc.py ../src/template/RRC.m all
    ```
- Run Murphi (includes .m -> .cpp -> executable -> run the executable).
    ```
    python3 runMurphi.py all
    ```

## File organizations

- Handwritten files:
  - Scripts
    ```
    scripts/
    ```
  - Murphi source file templates
    ```
    src/template/
    ```
  - Litmus test config templates
    ```
    configs/template/
    ```
- Generated files:
  - Murphi source files
    ```
    src/
    ```
  - Configs
    ```
    configs/*.yml
    ```
  - Murphi-generated cpp and executable
    ```
    build/
    ```

## Litmus tests
- For each template litmus test config listed below, the config generation script enumerates 1) the mapping from address to cache directory; 2) store types (nt or reg); 3) whether counters and timestamps will overflow, and generates a test config for each of the combination. For example, in a test config named ```IRIW.a2d0_1.st11.of.yml```, ```IRIW``` says the config is a Independent-Read-of-Independent-Write litmus test. ```a2d0_1``` says address ```0``` and ```1``` are mapped to different cache directories(as they are separated by ```_```). ```st11``` says that both stores in the config are regular stores (```0``` is non-temporal store ```1``` is regular store). ```of``` says the config triggers timestamp or counter overflow.
- Armv8 litmus tests
    ```
    2+2W
    CoRR1
    CoRR2
    CoRR3
    CoRW
    CoWR
    CoWW
    IRIW
    LB
    MP
    R
    RWC
    S
    SB
    WRC
    WRW+2W
    WWC
    ```
- Handcrafted litmus tests
  ```
  PC_2cl
  PC_bar
  PC_dbst
  PC_mtrl
  ```