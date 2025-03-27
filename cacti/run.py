import os
import re
import pandas as pd

def process_res(filename):
    patterns = [
        "Fully associative cache array: Area \(mm2\): ([\d\.]+)",
        "Total leakage power of a bank \(mW\): ([\d\.]+)",
        "Total dynamic read energy per access \(nJ\): ([\d\.]+)",
        "Total dynamic write energy per access \(nJ\): ([\d\.]+)",
    ]
    
    results = []
    
    with open(filename, 'r') as file:
        content = file.read()
        for pattern in patterns:
            match = re.search(pattern, content)
            if match:
                results.append(float(match.group(1)))
    
    return results

if __name__ == '__main__':
    os.system("./cacti -infile sample_config_files/L1cache.cfg  > res1.txt 2>/dev/null")
    os.system("./cacti -infile sample_config_files/procStRlxCnts.cfg  > res2.txt 2>/dev/null")
    os.system("./cacti -infile sample_config_files/procUncommittedEpochs.cfg  > res3.txt 2>/dev/null")
    
    os.system("./cacti -infile sample_config_files/LLCcache.cfg  > res4.txt 2>/dev/null")
    os.system("./cacti -infile sample_config_files/dirstRlxCnts.cfg  > res5.txt 2>/dev/null")
    os.system("./cacti -infile sample_config_files/dirNotifyCnts.cfg  > res6.txt 2>/dev/null")
    os.system("./cacti -infile sample_config_files/dirMaxCommittedEpochs.cfg  > res7.txt 2>/dev/null")
    
    components = ['store counter', 'unAck-ed epoch',
                  'store counter', 'notification counter', 'largest Comm. epoch']
    res_files = ['res2.txt', 'res3.txt', 'res5.txt', 'res6.txt', 'res7.txt']
    sizes = ['8', '8', '8*16', '16*16', '8']
    data = []
    for component, res_file, size in zip(components, res_files, sizes):
        area, power, r_energy, w_energy = process_res(res_file)
        data.append([component, size, area, power, r_energy, w_energy])
    data.insert(0, ['Processor (total)', '', data[0][2] + data[1][2],
                    data[0][3] + data[1][3], data[0][4] + data[1][4], data[0][5] + data[1][5],])
    data.insert(3, ['Directory (total)', '', data[3][2] + data[4][2] + data[5][2],
                    data[3][3] + data[4][3] + data[5][3], data[3][4] + data[4][4] + data[5][4],
                    data[3][5] + data[4][5] + data[5][5],])
    for i,row in enumerate(data):
        if row[0].find('total') != -1:
            data[i][4] = ''
        else:
            r = row[4]
            w = row[5]
            data[i][4] = f'{r}/{w}'
        data[i] = data[i][:-1]
    df = pd.DataFrame(data, columns=['Component', 'Size (entries)', 'Area (mm2)', 'Power (mW)', 'Acc. Energy (r/w nJ)'])
    df.to_csv('../figures/Table 3.csv', index=False)