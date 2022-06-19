# -*- coding: utf-8 -*-
"""
Created on Fri Mar 18 10:38:44 2022

@author: edemirovic
"""

'''
GOALS

run solver with different parameters and compare performance
run solver against other solver and compare performance
'''

import shutil
import subprocess
import os
import sys
import statistics
from datetime import datetime

def WriteTableToCSVFile(table, output_file):
    with open(output_file, "w") as fp:
        for row_index in range(len(table)):
            row = table[row_index]
            fp.write(row[0])
            for i in range(1, len(row)):
                fp.write(", {}".format(row[i]))
            
            if row_index + 1 != range(len(table)):
                fp.write('\n')

#takes as input a filer produced by Pumpkin
#and parses relevant statistics
def ReadPumpkinOutputFile(file_location):
    file_content = []
    with open(file_location, "r") as fp:
        file_content = fp.readlines()
    
    stats = {'solutions':[]}
    stats['feasible'] = 0
    stats['best-objective'] = float('inf')
        
    i = 0
    while i < len(file_content):
        line = file_content[i]        
        if 'c File: ' in line:
            temp = line.split()
            assert(len(temp) >= 3)
            stats['file'] = temp[2]
            #in case the file location has spaces, all of these need to be merged
            for i in range(3, len(temp)):
                stats['file'] += temp[i]            
        elif 'c conflicts until restart:' in line:
            temp = line.split()
            assert(len(temp) == 5)
            num_conflicts_until_restart = int(temp[4])
            i += 1
            line = file_content[i]
            assert('c restart counter:' in line)
            temp = line.split()
            assert(len(temp) == 4)
            restart_counter = int(temp[3])
            i += 1
            line = file_content[i]            
            if 'c linear search initial bound' not in line and 'timeout' not in line and 'c preprocessor harden' not in line:
                if len(stats['solutions']) > 0:
                    print(line)
                    assert('c not debug checking satisfaction of encoded constraints..' in line)
                    i += 1
                    line = file_content[i]
                
                objective_cost = None
                time_to_sol = None
                if 'o ' in line:
                    temp = line.split()
                    assert(len(temp) == 2)
                    objective_cost = int(temp[1])
                    i += 1
                    line = file_content[i]
                    assert('c t = ' in line)
                    temp = line.split()
                    assert(len(temp) == 4)
                    time_to_sol = int(temp[3])                
                entry = {'objective':objective_cost, 
                         'restart-counter':restart_counter, 
                         'num-conflicts-until-restart':num_conflicts_until_restart,
                         'time':time_to_sol}            
                stats['solutions'].append(entry)    
        elif 'c Time spent reading the file:' in line:
            temp = line.split()
            assert(len(temp) == 7)
            stats['time-reading-file'] = float(temp[6])
        elif 'c num lexicographical objectives:' in line:
            temp = line.split()
            assert(len(temp) == 5)
            stats['num-lexi-objectives'] = int(temp[4])    
        elif 'c blocked restarts:' in line:
            temp = line.split()
            assert(len(temp) == 4)
            stats['num-blocked-restarts'] = int(temp[3])      
        elif 'c blocked restarts:' in line:
            temp = line.split()
            assert(len(temp) == 4)
            stats['num-blocked-restarts'] = int(temp[3])   
        elif 'c nb removed clauses:' in line:
            temp = line.split()
            assert(len(temp) == 5)
            stats['num-removed-clauses'] = int(temp[4])   
        elif 'c nb learnts DL2:' in line:
            temp = line.split()
            assert(len(temp) == 5)
            stats['num-learnt-size2'] = int(temp[4])   
        elif 'c nb learnts size 1:' in line:
            temp = line.split()
            assert(len(temp) == 6)
            stats['num-learnt-size1'] = int(temp[5])   
        elif 'c nb learnts size 3:' in line:
            temp = line.split()
            assert(len(temp) == 6)
            stats['num-learnt-size3'] = int(temp[5])  
        elif 'c nb learnts:' in line:
            temp = line.split()
            assert(len(temp) == 4)
            stats['num-learnt-clauses'] = int(temp[3])  
        elif 'c avg learnt clause size:' in line:
            temp = line.split()
            assert(len(temp) == 6)
            stats['avg-learnt-clause-size'] = 0
            if 'nan(ind)' not in temp[5]:
                stats['avg-learnt-clause-size'] = float(temp[5])
        elif 'c current number of learned clauses:' in line:
            temp = line.split()
            assert(len(temp) == 7)
            stats['num-learnt-clauses'] = int(temp[6])
        elif 'c ratio of learned clauses:' in line:
            temp = line.split()
            assert(len(temp) == 6)
            stats['ratio-learnt-clauses'] = float(temp[5])
        elif 'c conflicts:' in line:
            temp = line.split()
            assert(len(temp) == 3)
            stats['num-conflicts'] = int(temp[2])
        elif 'c decisions:' in line:
            temp = line.split()
            assert(len(temp) == 3)
            stats['num-decisions'] = int(temp[2])
        elif 'c propagations:' in line:
            temp = line.split()
            assert(len(temp) == 3)
            stats['num-propagations'] = int(temp[2])          
        elif 'c Primal integral:' in line:
            temp = line.split()
            assert(len(temp) == 4)
            stats['primal-integral'] = float(temp[3])   
        elif 'c wallclock time:' in line:
            temp = line.split()
            assert(len(temp) == 5)
            stats['time-wallclock'] = int(temp[3])   
        elif 'c CPU time:' in line:
            temp = line.split()
            assert(len(temp) == 4)
            stats['time-cpu'] = float(temp[3])
        elif 'ERROR' in line or 'error' in line or 'Error' in line or 'Solution not OK' in line:
            print("ERROR in file {}".format(file_location))
            exit(1)            
        i += 1
        
    if len(stats['solutions']) > 0:        
        objectives = [x['objective'] for x in stats['solutions'] if x['objective'] is not None]
        if len(objectives) > 0:
            stats['best-objective'] = min(objectives)
            stats['feasible'] = 1
        
    #sanity check
    sols = stats['solutions']
    if len(sols) > 0:
        for i in range(len(sols)-1):
            assert(sols[i]['objective'] is None or sols[i]['objective'] > sols[i+1]['objective'])
            assert(sols[i]['time'] is None or sols[i]['time'] <= sols[i+1]['time'])
            assert(sols[i]['restart-counter'] < sols[i+1]['restart-counter'])
        
    #print(stats)
    return stats

#runs the Pumpkin solver with given parameters
#the file location is expected in the parameters
#if the file_to_print_output parameter is given
#   prints the output of the solver to the specified file
#the method ReadPumpkinOutputFile may be used to parse the output
def RunSolver(solver_location, parameters, file_to_print_output = ""):
    #prepare the parameters that will be passed to the solver
    args = [solver_location]
    for p in parameters.keys():
        args.append("-{}".format(p))
        args.append(str(parameters[p]))
    #run the solver
    popen = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output = popen.communicate()[0]        
    #print to file if needed
    if file_to_print_output != "":
        contents = output.decode("utf-8").splitlines()
        with open(file_to_print_output, "w") as fp:
            for i in range(len(contents)):
                line = contents[i]
                fp.write(line)
                if i + 1 != len(contents):
                    fp.write("\n")
    
#instance folder contains folders with instances
#this method runs all instances with the given solver and stores the outputs of the solver
#file parameter name is used so that different solvers can be used, e.g., each solver might use a different parameter to specify the file location
#   similarly for the seed name. Each instance is then run as many times as there are seed values.
#   I guess this can be a problem for solvers which do not specify an explicit file or seed parameter...but could handle those as a special case
def RunBatchExperiments(solver_location, parameters, input_folder, output_folder, seed_parameter_name, random_seed_values, file_parameter_name):
    #create the output folder
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)
        
    files = sorted(os.listdir(input_folder))
    print("Running experiments on {} files".format(len(files)))
    #read one file at a time
    for k in range(len(files)):
        file = files[k]
        
        instance_location = os.path.join(input_folder, file)
        instance_location = os.path.abspath(instance_location)
        
        parameters[file_parameter_name] = instance_location
                
        print("\tFile {} out of {}".format(k+1, len(files)))
        #run the instance the given number of times
        #   storing the result in separate files
        
        #create the folder where the results will be stored
        output_instance_folder = os.path.join(output_folder, file)
        if not os.path.exists(output_instance_folder):
            os.makedirs(output_instance_folder)
        
        for i in range(len(random_seed_values)):
            seed = random_seed_values[i]
            
            parameters[seed_parameter_name] = seed
            
            output_file = os.path.join(output_instance_folder, file)
            output_file += "-{}.txt".format(i)
            
            RunSolver(solver_location, parameters, output_file)
            
    print("Experiments completed!")
    
#the folder_with_results stores results in folders
#   where each folder name is the instance name
#   and each file within the folder is a run of the algorithm
def ReadBatchExperimentsPumpkin(folder_with_results):
    raw_results = {}    
    instance_names = sorted(os.listdir(folder_with_results))
    for instance_name in instance_names:

        instance_folder = os.path.join(folder_with_results, instance_name)

        for result_file in os.listdir(instance_folder):
            
            file_location = os.path.join(instance_folder, result_file)
            file_location = os.path.abspath(file_location)
            
            stats = ReadPumpkinOutputFile(file_location)
                 
            if instance_name not in raw_results:
                raw_results[instance_name] = []                
            raw_results[instance_name].append(stats)
    return raw_results
    
#an array with raw results
#summarise each raw result
#and then compare?
#infinity values are ignored?
#todo describe
def SummariseBatchExperiments(raw_results, metrics):
    summary = {}
    for file_name in raw_results:
        summary[file_name] = {}
        for metric in metrics:
            values = [single_run[metric] for single_run in raw_results[file_name] if single_run[metric] != float('inf')]
            avg_value = float('inf')
            if len(values) > 0:
                avg_value = statistics.mean(values) 
            summary[file_name][metric + '-avg'] = avg_value
        '''
        objective_values = []
        propagations = []
        for single_run in raw_results[file_name]:
            if len(single_run['solutions']) > 0:
                objective_values.append(single_run['solutions'][-1]['objective'])
            
            if 'num-propagations' in single_run:
                propagations.append(single_run['num-propagations'])
        
        num_feasible = 0
        avg_objective = float('inf')
        
        if len(objective_values) > 0:        
            #num_runs = len(raw_results[file_name])
            num_feasible = len(objective_values)
            avg_objective = statistics.mean(objective_values)
        
        avg_propagations = float('inf')
        
        if len(propagations) > 0:        
            avg_propagations = statistics.mean(propagations)
        
        summary[file_name] = {'avg-objective':avg_objective, 'num-feasible':num_feasible, 'avg-propagations':avg_propagations}
        '''
        
    return summary        
    
#receives as input an array, where each element is a [solver_name, summary]
#a summary is obtained using SummariseBatchExperiments
#then the solvers are compared
#metrics-minmax-info is an array where the i-th element is a pair [metric_name, max | min]
def CreateTableComparingSummaries(summaries, csv_file_location, metric_minmax_info):
    assert(len(summaries) >= 2)
    
    for m in metric_minmax_info:
        assert(m[1] == 'max' or m[1] == 'min')
        
    #make sure all summaries contain the same instances
    all_instances = set()    
    for i in range(len(summaries)):
        instances = [str(x) for x in summaries[i][1]]
        all_instances.update(instances)
    
    for entry in summaries:
        if len([x for x in entry[1]]) != len(all_instances):
            print("ERROR: {} does not have results for all {} instances!".format(entry[0], len(all_instances)))
            exit(1)        
            
    all_instances = list(all_instances)
    
    table = []
    header = ['instance'] # + [x[0] for x in summaries]
    for p in metric_minmax_info:
        header += [p[0]]
        header += ["" for x in range(len(summaries)-1)]
        
    table.append(header)
    
    header = [""]
    for m in metric_minmax_info:
        for s in summaries:
            header += [s[0]]
    table.append(header)
    
    for inst in all_instances:
        row = [inst]
        for p in metric_minmax_info:
            metric = p[0]
            #find the value of the best performing solver
            best_value = min([x[1][inst][metric + '-avg'] for x in summaries])
            if p[1] == 'max':
                best_value = max([x[1][inst][metric + '-avg'] for x in summaries])
            
            for x in summaries:
                val = x[1][inst][metric + "-avg"]
                score = best_value / val
                if p[1] == 'max':
                    score = val / best_value
                score = round(score, 2)
                row.append(score)  
        
        table.append(row)
            
    footer = ['total']
    for k in range(len(metric_minmax_info)):    
        for i in range(len(summaries)):
            total_score = 0
            for j in range(2, len(table)): # goes from 2 since the first two rows are headers
                #print(1+k*len(metric_minmax_info)+i)
                #print(len(table[j]))
                total_score += table[j][1+k*len(summaries)+i] #+1 since the first column is the instance name
            total_score /= len(all_instances)
            footer.append(total_score)
    table.append(footer)
    
    WriteTableToCSVFile(table, csv_file_location)
    
#the solver trajectory is a list of dictionaries
#where each dictonary has the following structure
#   {'objective':objective_cost, 
#   'restart-counter':restart_counter, 
#   'num-conflicts-until-restart':num_conflicts_until_restart,
#   'time':time_to_sol}            
#the method compares if there trajectories look the same
#this is useful when changes to the solver are made that only increase the efficiency of the solver but do not change the trail behaviour
def AreSolverTrajectoriesSame(trajectory1, trajectory2):
    for i in range(min(len(trajectory1), len(trajectory2))):
        if trajectory1[i]['objective'] != trajectory2[i]['objective']:
            return False
        elif trajectory1[i]['restart-counter'] != trajectory2[i]['restart-counter']:
            return False
        elif trajectory1[i]['num-conflicts-until-restart'] != trajectory2[i]['num-conflicts-until-restart']:
            return False
    return True 

def DoBatchExperimentsHaveSameTrajectories(raw_results1, raw_results2):
    #check if the two batches contain the same files
    assert(set([f for f in raw_results1]) == set([f for f in raw_results2]))
    
    for file_name in raw_results1:
        #each batch should have the same number of runs for the instance
        assert(len(raw_results1[file_name]) == len(raw_results2[file_name]))
                
        for i in range(len(raw_results1[file_name])):
            trajectory1 = raw_results1[file_name][i]['solutions']
            trajectory2 = raw_results2[file_name][i]['solutions']
            if not AreSolverTrajectoriesSame(trajectory1, trajectory2):
                return False
            
    return True