#!/bin/bash

import os
import sys

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import statistics
import copy 
from enum import Enum
from statistics import median

from dataclasses import dataclass

def get_data(data_file, header, sep=" "):
    """
    Spalte1 Spalte2
    File:
    data1 data2
    data3 data4
    """

    df = pd.read_csv(data_file, sep=sep, names=header)

    return df


def plot_box_plot(label, col, out_file, df, sol_status):
    fig, ax1 = plt.subplots(figsize=(5,4))

    ax1.boxplot(df[col].values, labels=[sol_status.value])
    ax1.set_title(label)

    plt.rcParams.update({
        "text.usetex": True,
        "mathtext.fontset": "cm",
        "font.family": "STIXGeneral"
        #"font.size": 22
        #"font.sans-serif": "Helvetica",
    })

    plt.grid(True)

    plt.show()
    plt.savefig(out_file)

class SolStatus(Enum):
    UNKNOWN="unknown"
    SAT="sat"
    UNSAT="unsat"
    ALL="all"

def filter_solution_status(df, sol_status, sol_status_file):
    drop = []
    sol_status_df = get_data(sol_status_file, header=["instance", "time", "status"])

    matching_instances = []

    for idx, row in sol_status_df.iterrows():
        if row["status"] == sol_status.value:
            matching_instances.append(row["instance"])

    matching_instances = set(matching_instances)
    
    for idx, row in df.iterrows():
        if row["instance"] not in matching_instances:
            drop.append(idx)

    if len(drop) > 0:
        print(drop)
        df.drop(drop, inplace=True)

def plot_boxplot_dup_reports_ratios(out_file, data_file, sol_status=SolStatus.ALL, sol_status_file=None):
    df = get_data(data_file, header=["instance", "reports", "hashes", "dup_reports", "dup_hashes"])
   
    dup_reports = min(df["dup_reports"].values)
    dup_hashes = median(df["hashes"].values)

    print("dup reports min: %s" % dup_reports)
    print("dup hashes median: %s" % dup_hashes)

    if sol_status!=SolStatus.ALL:
        filter_solution_status(df, sol_status, sol_status_file) 

    df["dup_reports_ratio"] = df["dup_reports"] / df["reports"]
    plot_box_plot("Boxplot $1-\#\mathcal{H}/\#\mathcal{L}$", "dup_reports_ratio", out_file, df, sol_status)


def plot_boxplot_dup_hashes_ratios(out_file, data_file, sol_status=SolStatus.ALL, sol_status_file=None):
    df = get_data(data_file, header=["instance", "reports", "hashes", "dup_reports", "dup_hashes"])

    if sol_status!=SolStatus.ALL:
        filter_solution_status(df, sol_status, sol_status_file) 

    df["dup_hashes_ratio"] = df["dup_hashes"] / df["hashes"]
    plot_box_plot("Boxplot $\#\operatorname{duph}(\mathcal{H},\mathcal{L})/\#\mathcal{H}$", "dup_hashes_ratio", out_file, df, sol_status)


def plot_cdf(label, out_file, col, df, sol_status_file):
    sol_status_df = get_data(sol_status_file, header=['instance', 'time', 'status'])

    colors = ["blue", "orange", "gray", "cyan"]

    fig, ax1 = plt.subplots(figsize=(5,4))
    for jdx, s in enumerate(SolStatus):
        instances = []
        if s == SolStatus.ALL:
            instances = sol_status_df["instance"].values
        else: 
            instances = sol_status_df.loc[sol_status_df["status"] == s.value]["instance"].values

        values = sorted(df.loc[df["instance"].isin(instances)][col].values)

        x=[]
        prob=[]

        for idx, value in enumerate(values):
            x.append(value)
            prob.append(idx+1)

        if len(x) > 0 and x[-1] < 1:
            x.append(1)
            prob.append(len(values))

        for idx, p in enumerate(prob):
            prob[idx] = p/len(values)

        print("test")
        ax1.plot(x, prob, label=s.value, linewidth=3, color="tab:%s" % colors[jdx])
    
    ax1.set_xlabel("$x$")
    ax1.set_ylabel("$P[X\leq x]$")
    ax1.set_title("CDF for " + label) 
    ax1.legend(loc="lower right")

    plt.rcParams.update({
        "text.usetex": True,
        "mathtext.fontset": "cm",
        "font.family": "STIXGeneral"
        #"font.size": 22
        #"font.sans-serif": "Helvetica",
    })
    
    plt.grid(True)
    plt.show()
    plt.savefig(out_file)


def plot_cdf_dup_hashes_ratios(out_file, data_file, sol_status_file):
    
    df = get_data(data_file, header=["instance", "reports", "hashes", "dup_reports", "dup_hashes"])

    df["dup_hashes_ratio"] = df["dup_hashes"] / df["hashes"]
    plot_cdf("$\#\operatorname{duph}(\mathcal{H},\mathcal{L})/\#\mathcal{H}$", out_file, "dup_hashes_ratio", df, sol_status_file)


def plot_cdf_dup_reports_ratios(out_file, data_file, sol_status_file):
    
    df = get_data(data_file, header=["instance", "reports", "hashes", "dup_reports", "dup_hashes"])

    df["dup_reports_ratio"] = df["dup_reports"] / df["reports"]
    plot_cdf("$1-\#\mathcal{H}/\#\mathcal{L}$", out_file, "dup_reports_ratio", df, sol_status_file)



def plot_time_dup(out_file, data_file):
    
    df = get_data(data_file, header=["t_f", "t", "hash", "lbd", "len", "process", "solver"])

    time = df["t"].values

    time = sorted(time)

    x=[]
    ratio=[]

    for idx, t in enumerate(time):
        if idx > 0 and x[-1] == t:
            ratio[-1] += 1
        else:
            x.append(t)
            ratio.append(idx)

    for idx, t in enumerate(ratio):
        ratio[idx] = t/len(time) # duplicates at time t / all duplicates

    fig, ax1 = plt.subplots()

    ax1.plot(x, ratio, "-")
    ax1.set_xlabel("time $t$ [$s$]")
    ax1.set_ylabel("duplicates at time $t$ / all duplicates")
    ax1.set_title("duplicates over time")

    plt.show()
    plt.savefig(out_file)

@dataclass
class TimeDataPoint:
    t: float
    avg_x: float
    max_x: float
    min_x: float

def plot_time_dup_all_cloud(out_file, data_inst_dir, max_instance, data_filename):
   
    df2 = get_data(os.path.join(data_inst_dir, "qualified-runtimes-and-results.txt"), header=["instance", "runtime", "status"])

    print("time dup all plot")

    f = dict()

    max_instance = int(max_instance)
    print(max_instance)

    runtimes = df2["runtime"].values

    for instance in range(1,max_instance+1):
        print(instance)
        path = os.path.join(data_inst_dir, str(instance), data_filename)
        print(path)
        df = get_data(path, header=["t"])

        execution_time = runtimes[instance-1]
        print(execution_time)
       
        #df["t"] = df["t"].apply(lambda x: round(x/10, 0)*10)
        ts = df["t"].values
        j = 0

        dc = []
        uts = []

        print("%s %s" % (ts[-1], execution_time))
        if len(ts) > 0:
            execution_time = max(ts[-1], execution_time)
        assert(len(ts) == 0 or ts[-1] <= execution_time)


        if len(ts) == 0 or ts[0] > 0:
            # insert start point
            dc.append(0)
            uts.append(0)

        last_t = -1
        for idx, t in enumerate(ts):
            t = round(t/execution_time, 1) # auf 10 sec schritte
            if last_t != t:
                dc.append((idx+1)/len(ts))
                uts.append(t)
                last_t = t
            else:
                dc[-1] += 1/len(ts)
                        
        # cluster 
        for idx, t in enumerate(uts):
            dc[idx] = round(dc[idx], 2)

            if (t,dc[idx]) not in f.keys():
                f[(t,dc[idx])] = 1
            else:
                f[(t,dc[idx])] += 1

        print("size %s" % len(f))
        
    print("Plotting")

    fig, ax1 = plt.subplots()
    
    keys = list(f.keys())
    ax1.scatter([t for t,_ in keys], [x for _,x in keys], s=[(f[k]/10)**2 for k in keys])
    ax1.set_xlabel("relative running time $t$")
    ax1.set_ylabel("$\# \operatorname{duph}(\mathcal{H},\mathcal{L})/ \# \mathcal{H}$")
    ax1.set_title("$\#$ instances with $\operatorname{duph}(\mathcal{H},\mathcal{L})/ \# \mathcal{H}$ until $t$")

    plt.rcParams.update({
        "text.usetex": True,
        "mathtext.fontset": "cm",
        "font.family": "STIXGeneral"
        #"font.size": 22
        #"font.sans-serif": "Helvetica",
    })
    


    plt.show()
    plt.savefig(out_file)




def plot_time_dup_all(out_file, data_inst_dir, max_instance, data_filename):
   
    df2 = get_data(os.path.join(data_inst_dir, "qualified-runtimes-and-results.txt"), header=["instance", "runtime", "status"])

    print("time dup all plot")

    f = list()
    f.append(TimeDataPoint(0.0, 0.0, 0.0, 2.0))

    max_instance = int(max_instance)
    print(max_instance)

    def add_data_point(j, old, f, t, dup, n):
        if j > 0:
            f.insert(j, TimeDataPoint(t, (old.avg_x * (n-1) + dup) / n, max([old.max_x, dup]), min([old.min_x, dup])))
        else:
            f.insert(j, TimeDataPoint(t, dup, dup, dup))

    def change_data_point(j, f, dup, n):
            f[j].avg_x = (f[j].avg_x * (n-1) + dup) / n
            f[j].max_x = max([f[j].max_x, dup])
            f[j].min_x = min([f[j].min_x, dup])


    runtimes = df2["runtime"].values

    for instance in range(1,max_instance+1):
        print(instance)
        path = os.path.join(data_inst_dir, str(instance), data_filename)
        print(path)
        df = get_data(path, header=["t"])

        execution_time = runtimes[instance-1]
        print(execution_time)
         
        ts = df["t"].values
        j = 0

        dc = []
        uts = []

        print("%s %s" % (ts[-1], execution_time))
        if len(ts) > 0:
            execution_time = max(ts[-1], execution_time)
        assert(len(ts) == 0 or ts[-1] <= execution_time)


        if len(ts) == 0 or ts[0] > 0:
            # insert start point
            dc.append(0)
            uts.append(0)

        last_t = -1
        for idx, t in enumerate(ts):
            if last_t == t:
                dc[-1] += 1/len(ts)
            else:
                dc.append(idx/len(ts))
                uts.append(t/execution_time)
                last_t = t

        old_fj = f[j]
        for idx, t in enumerate(uts):
            if j == len(f):
                # insert new data point
                add_data_point(j, old_fj, f, t, dc[idx], instance)
                # increase j by 1
                j += 1


            # j is pointing to f[j].t >= t
            else:
                assert(f[j].t >= t)
                if f[j].t > t:
                    # add new data point because f does not know t
                    add_data_point(j, old_fj, f, t, dc[idx], instance)
                    assert(f[j].t == t)
                    j += 1
                
                # f[j] == t
                #assert(f[j] == t)
                # adapt f[j] subject to t until f[j].t >= next_t
                while j < len(f) and (idx+1 >= len(uts) or f[j].t < uts[idx+1]):
                    # adapt f[j]
                    old_fj = copy.deepcopy(f[j])
                    change_data_point(j, f, dc[idx], instance)
                    

                    j += 1


    print("Plotting")

    fig, ax1 = plt.subplots()

    ax1.plot([x.t for x in f], [x.avg_x for x in f], "-")
    ax1.set_xlabel("relative run time")
    ax1.set_ylabel("avg. $\# dup. hashes/ \# hashes$")

    plt.show()
    plt.savefig(out_file)


def plot_time_until_dup(out_file, data_file):
    
    df = get_data(data_file, header=["t_f", "t", "hash", "lbd", "len", "process", "solver"])

    df["t"] = df["t"] - df["t_f"]
    time = df["t"].values
    time = sorted(time)

    x=[]
    ratio=[]

    for idx, t in enumerate(time):
        if idx > 0 and x[-1] == t:
            ratio[-1] += 1
        else:
            x.append(t)
            ratio.append(idx)

    for idx, t in enumerate(ratio):
        ratio[idx] = t/len(time) # duplicates at time t / all duplicates

    fig, ax1 = plt.subplots()

    ax1.plot(x, ratio, "-")
    ax1.set_xlabel("duration until duplicate appearance $t-t_f$ [$s$]")
    ax1.set_ylabel("duplicates with duration at most $t-t_f$ / all duplicates")
    ax1.set_title("duration of duplicate appearance since first appearance")

    plt.show()
    plt.savefig(out_file)

def plot_heatmap_dup_hash_ratios(out_file, data_file):

    df = get_data(data_file, header=["p1", "s1", "p2", "s2", "dup", "amount"])

    print("read data")

    max_p = 2
    max_s = 31
    n = (1+min(max_p, df["p1"].max()))*(1+min(max_s, df["s1"].max()))
    arr = [[[] for j in range(n)] for i in range(n)]

    for i in range(n):
        arr[i][i].append(0) # trivial

    solvers = df["s1"].max()+1

    def idx(process, solver):
        return process*solvers + solver

    for i, row in df.iterrows():
        if row["p1"] > max_p or row["s1"] > max_s or row["p2"] > max_p or row["s2"] > max_s:
                continue
        ratio = 0
        if row["amount"] > 0:
            ratio = row["dup"]/row["amount"]
        arr[idx(row["p1"], row["s1"])][idx(row["p2"], row["s2"])].append(ratio)
    
    print("insert data")

    median_arr = np.zeros((n,n))

    for i in range(n):
        for j in range(n):
            if len(arr[i][j]) == 0:
                print("%s,%s is empty" % (i,j))
                arr[i][j].append(0)
            median_arr[i][j] = statistics.median(arr[i][j])

    #fig, ax1 = plt.subplots(figsize=(14, 14))
    fig, ax1 = plt.subplots(figsize=(12,10))
    im = ax1.imshow(median_arr)

    #labels = ["(%s, %s)" % (p, s) if s % 3 == 0 else "" for p in range(min(max_p, df["p1"].max())+1) for s in range(min(max_s, df["s1"].max())+1)]
    solvers = min(max_s, df["s1"].max())+1
    def get_label(p, s):
        if (p*solvers + s) % 3 == 0:
            return "(k,%s,%s)" % (p, s)
        #elif p*ps + s == 1:
        #    return "(c,%s,%s)" % (p, s)
        #elif p*ps + s == 2:
        #    return "(l,%s,%s)" % (p, s)
        else:
            return ""

    labels = [get_label(p, s) for p in range(min(max_p, df["p1"].max())+1) for s in range(min(max_s, df["s1"].max())+1)]
    ax1.set_xticks(np.arange(n), labels=labels)
    ax1.set_yticks(np.arange(n), labels=labels)

    # Rotate the tick labels and set their alignment.
    plt.setp(ax1.get_xticklabels(), rotation=45, ha="right", rotation_mode="anchor")

    #for i in range(n):
    #    for j in range(n):
    #        text = ax1.text(j, i, round(median_arr[i, j], 3), ha="center", va="center", color="w")

    cbarlabel="" #"#H_1\cap H_2/#H_1\cup H_2$"
    cbar = ax1.figure.colorbar(im, ax=ax1)
    cbar.ax.set_ylabel(cbarlabel, rotation=-90, va="bottom")

    ax1.set_title("$\#\mathcal{H}_S\cap\mathcal{H}_T / \# \mathcal{H}_S\cup\mathcal{H}_T$", fontsize=25)
    fig.tight_layout()

    plt.rcParams.update({
        "text.usetex": True,
        "mathtext.fontset": "cm",
        "font.family": "STIXGeneral",
        #"font.sans-serif": "Helvetica",
    })

    plt.show()
    plt.savefig(out_file)


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python3 %s <plot> <outfile> <data-file>" % sys.argv[0])
        print("Usage: python3 %s <plot> <outfile> <data-file> <sol-status-file>" % sys.argv[0])
        print("Usage: python3 %s <plot> <outfile> <data-file> <sol_status> <sol-status-file>" % sys.argv[0])
    elif len(sys.argv) == 4:
        if sys.argv[1] == "--plot-boxplot-dup-hashes-ratios":
            plot_boxplot_dup_hashes_ratios(sys.argv[2], sys.argv[3])
        elif sys.argv[1] == "--plot-boxplot-dup-reports-ratios":
            plot_boxplot_dup_reports_ratios(sys.argv[2], sys.argv[3])
        elif sys.argv[1] == "--plot-heatmap-dup-hash-ratios":
            plot_heatmap_dup_hash_ratios(sys.argv[2], sys.argv[3])
        elif sys.argv[1] == "--plot-time-dup":
            plot_time_dup(sys.argv[2], sys.argv[3])
        elif sys.argv[1] == "--plot-time-until-dup":
            plot_time_until_dup(sys.argv[2], sys.argv[3])
        else:
            print("No plot specified!")
    elif len(sys.argv) == 5:
        if sys.argv[1] == "--plot-cdf-dup-hashes-ratios":
            plot_cdf_dup_hashes_ratios(sys.argv[2], sys.argv[3], sys.argv[4])
        elif sys.argv[1] == "--plot-cdf-dup-reports-ratios":
            plot_cdf_dup_reports_ratios(sys.argv[2], sys.argv[3], sys.argv[4])
    elif len(sys.argv) == 6:
        if sys.argv[1] == "--plot-boxplot-dup-hashes-ratios":
            plot_boxplot_dup_hashes_ratios(sys.argv[2], sys.argv[3], SolStatus(sys.argv[4]), sys.argv[5])
        elif sys.argv[1] == "--plot-boxplot-dup-reports-ratios":
            plot_boxplot_dup_reports_ratios(sys.argv[2], sys.argv[3], SolStatus(sys.argv[4]), sys.argv[5])
        elif sys.argv[1] == "--plot-time-dup-all-cloud":
            plot_time_dup_all_cloud(*sys.argv[2:6])

