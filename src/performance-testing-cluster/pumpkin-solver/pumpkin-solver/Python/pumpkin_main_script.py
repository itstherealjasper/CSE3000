# -*- coding: utf-8 -*-
"""
Created on Fri Mar 18 16:09:03 2022

@author: edemirovic
"""

from pumpkin_experiments import *
import pandas as pd
 
parameters = {'time':5}    
date1 = datetime.now().strftime("%m-%d-%Y-%H-%M-%S")


RunBatchExperiments(solver_location="SATsolver.exe", 
                    parameters=parameters,
                    input_folder="hehe",
                    output_folder="time5-"+date1,
                    seed_parameter_name="seed",
                    random_seed_values=[-1, -1],
                    file_parameter_name="file"
                    )



exit(1)


parameters = {'time':10}    
date2 = datetime.now().strftime("%m-%d-%Y-%H-%M-%S")
date2 = "bub2"

'''
RunBatchExperiments(solver_location="SATsolver.exe", 
                    parameters=parameters,
                    input_folder="hehe",
                    output_folder="time10-"+date2,
                    seed_parameter_name="seed",
                    random_seed_values=[-1, -1],
                    file_parameter_name="file"
                    )
'''
b1 = ReadBatchExperimentsPumpkin("time5-"+date1)
b2 = ReadBatchExperimentsPumpkin("time10-"+date2)

metric_minmax_info = [['best-objective', 'min'], ['num-propagations', 'max'], ['feasible', 'max']]
metrics = [x[0] for x in metric_minmax_info]

s1 = SummariseBatchExperiments(b1, metrics)
s2 = SummariseBatchExperiments(b2, metrics)

print("SAME TRAJECTORIES {}".format(DoBatchExperimentsHaveSameTrajectories(b1, b2)))

summaries = [['time5', s1], ['time10', s2]]

CreateTableComparingSummaries(summaries=summaries, csv_file_location='time2vs3.csv', metric_minmax_info=metric_minmax_info)

# Reading the csv file
df_new = pd.read_csv('time2vs3.csv')
 
# saving xlsx file
GFG = pd.ExcelWriter('time2vs3.xlsx')
df_new.to_excel(GFG, index=False, header=True)
 
GFG.save()
