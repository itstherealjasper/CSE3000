import os

def generateStarexecScripts():
    for a in ['--cg-strat=0 --cg-indcores=0', '--cg-strat=1 --cg-indcores=1']:
        for b in ['--cg-cardreduct=bestbound', '--cg-cardreduct=minslack']:
            for c in ['--cg-encoding=lazysum', '--cg-encoding=reified']:
                for d in ['--opt-mode=coreguided', '--opt-mode=hybrid']:
                    with open('starexec_run_' + a.replace(' ', '') + b + c + d + ".sh", 'w') as fp:
                        fp.write('#!/bin/bash\n')
                        fp.write('delay=3\n')
                        fp.write('wl=$(($STAREXEC_WALLCLOCK_LIMIT-$delay))\n')
                        fp.write('timeout -s 15 $wl ./roundingsat {} {} {} {} $1'.format(a, b, c, d))


def processRoundingSATlog(log_file):
    bounds = {'UB':float('inf'), 'LB':0}
    #c bounds - >= 0
    with open(log_file, 'r') as fp:
        lines = fp.readlines()
        for line in lines:
            if 'c bounds ' in line:
                assert('>=' in line)
                #read the upper bound first
                i = line.find('c bounds ')
                assert(line[i] == 'c')
                i += len('c bounds ')
                #an upper bound is found, parse it
                if line[i] != '-':
                    ub = int(line[i:line.find(">=")])
                    assert(bounds['UB'] >= ub)
                    bounds['UB'] = ub
                #read the lower bound now
                i = line.find('>= ')
                i += len('>= ')
                assert('@' in line)
                #print(line)
                lb = int(line[i:line.find('@')])
                assert(lb >= bounds['LB'])
                bounds['LB'] = lb    
        
        if 'o ' in line[-2]:
            print("AN OPTIMAL SOLUTION HAS BEEN FOUND, CHECK THIS CASE")
            print(log_file)
            exit(1)
            
    return bounds  

def processLoandraLog(log_file):
    bounds = {'UB':float('inf'), 'LB':0}
    with open(log_file, 'r') as fp:
        line = fp.readline()
        while line:
            if 'c  Nb symmetry clauses:' in line:
                return bounds
            
            if 'c LB : ' in line:
                i = line.find('c LB : ')
                i += len('c LB : ')
                lb = int(line[i:line.find('CS : ')])
                assert(bounds['LB'] <= lb)
                bounds['LB'] = lb
            
            if 'c  Best solution:' in line:
                i = line.find('c  Best solution:')
                i += len('c  Best solution:')
                ub_str = line[i:].strip()
                if ub_str.isdigit():
                    assert(int(ub_str) <= bounds['UB'])
                    bounds['UB'] = int(ub_str)    
                return bounds
            line = fp.readline()
    return bounds

def ParsePumpkinLogFile(file_location):
    data = {}
    with open(file_location, 'r') as fp:
        line = fp.readline()
        while line:
            if 'rip' in line:
                print('It seems there is an error in the file {}'.format(file_location))
                exit(1)
            elif 'c num lexicographical objectives: ' in line:
                i = len('c num lexicographical objectives: ')
                data['num_lexicographical_objectives'] = int(line[i:])
            elif 'c encoding took ' in line:
                i = len('c encoding took ')
                j = line.find('seconds')
                data['ub_encoding_time'] = int(line[i:j])
            elif 'c conflicts: ' in line:
                i = len('c conflicts: ')
                data['num_conflicts'] = int(line[i:])
            elif 'c decisions: ' in line:
                i = len('c decisions: ')
                data['num_decisions'] = int(line[i:])
            elif 'c propagations: ' in line:
                i = len('c propagations: ')
                data['num_propagations'] = int(line[i:])
            elif 'c Primal integral: ' in line:
                i = len('c Primal integral: ')
                if 'inf' in line[i:]:
                    data['primal_integral'] = float(inf)                   
                else:
                    data['primal_integral'] = int(line[i:])                    
            elif line[0] == 'o' and line[1] == ' ' and not data.has_key('solutions'):
                data['solutions'] = [int(line[2:])]
                line = fp.readline()
                assert('c t = ' in line)
                i = len('c t = ')
                data['solutions_time'] = [int(line[i:])]                
            elif line[0] == 'o' and line[1] == ' ':
                obj = int(line[2:])
                assert(data['solutions'][-1] > obj)
                data['solutions'] += [obj]
                line = fp.readline()
                assert('c t = ' in line)
                i = len('c t = ')
                data['solutions_time'] += [int(line[i:])]                
            elif 'c CPU time: ' in line:
                i = len('c CPU time: ')
                data['CPU_time'] = int(line[i:])
            elif 'c wallclock time: ' in line:
                i = len('c wallclock time: ')
                data['wallclock'] = float(line[i:-1])
            elif 'c Time spent reading the file: ' in line:
                i = len('c Time spent reading the file: ')
                data['time_to_read_file'] = int(line[i:])
            line = fp.readline()

    return data

def ComputeScores(configs, benchmark_names, results):
    print("COMPUTING SCORES")
    
    scores = {config:0 for config in configs}
    count = 0
    for benchmark in benchmark_names:        
        #find the best upper bound first
        best_ub = float('inf')
        for c in configs:
            best_ub = min(results[c][benchmark]['UB'], best_ub)
            
        #and then assign a score for each solver
        for c in configs:
            
            if 'Loandra' in c and results['Loandra___default.sh'][benchmark]['UB'] != best_ub:
                print('{} = {} vs {}'.format(benchmark, results['Loandra___default.sh'][benchmark]['UB'], best_ub))
            
            if best_ub == float('inf'):
                scores[c] += 0
            else:            
                scores[c] += (float(best_ub+1) / (results[c][benchmark]['UB']+1))
            
    for c in configs:
        print("{}: {}".format(c, scores[c]))       
    
    print(count)

if __name__ == "__main__":

    benchmark_names = set()
    results = {}
    configs = os.listdir('mse18\\')

    configs[1] = 'RScg___--cg-strat=1--cg-indcores=1--cg-cardreduct=bestbound--cg-encoding=reified--opt-mode=hybrid.sh'
    configs = configs[0:2]
    #configs = configs[0:2]

    #read roundingSAT log data and store as results[config][benchmark] = objective_value
    for config in configs:
        
        if 'Loandra' in config:
            continue
        
        print(config)
        results[config] = {}
        for benchmark in os.listdir('mse18\\' + config):
            benchmark_names.add(benchmark)
            log_file = os.listdir('mse18\\' + config + '\\' + benchmark + '\\')[0]
            results[config][benchmark] = processRoundingSATlog('mse18\\' + config + '\\' + benchmark + '\\' + log_file)
            
    results['Loandra___default.sh'] = {}
    for benchmark in benchmark_names:
        log_file = os.listdir('mse18\\' + 'Loandra___default.sh' + '\\' + benchmark + '\\')[0]
        results['Loandra___default.sh'][benchmark] = processLoandraLog('mse18\\' + 'Loandra___default.sh' + '\\' + benchmark + '\\' + log_file)
        #print(results['Loandra___default.sh'][benchmark]['UB'])   
        
    print(len(benchmark_names))
    ComputeScores(configs, benchmark_names, results)