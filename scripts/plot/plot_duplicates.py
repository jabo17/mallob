#!/bin/bash

import os
import sys

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import statistics


def get_data(data_file, header, sep=" "):
    """
    Spalte1 Spalte2
    File:
    data1 data2
    data3 data4
    """

    df = pd.read_csv(data_file, sep=sep, names=header)

    return df

def plot_boxplot_dup_ratios(out_file, data_file):

    df = get_data(data_file, header=["instance", "dup", "amount", "ratio"])

    fig, ax1 = plt.subplots()

    ax1.boxplot(df["ratio"].values)
    ax1.set_title("duplicate produced clauses")

    plt.show()
    plt.savefig(out_file)


def plot_prob_dup_ratios(out_file, data_file):
    
    df = get_data(data_file, header=["instance", "dup", "amount", "ratio"])

    ratios = df["ratio"].values

    ratios = sorted(ratios)

    x=[]
    prob=[]

    for idx, ratio in enumerate(ratios):
        if idx > 0 and x[-1] == ratio:
            prob[-1] += 1
        else:
            x.append(ratio)
            print(ratio)
            prob.append(idx)

    for idx, p in enumerate(prob):
        prob[idx] = p/len(ratios)

    fig, ax1 = plt.subplots()

    ax1.plot(x, prob, "-o")
    ax1.set_xlabel("duplicate ratio $x$")
    ax1.set_ylabel("$P[x\leq x]$")
    ax1.set_title("$P[X\leq x]$ for duplicate ratio $x$")

    plt.show()
    plt.savefig(out_file)


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

    n = (1+df["p1"].max())*(1+df["s1"].max())
    arr = [[[] for j in range(n)] for i in range(n)]

    for i in range(n):
        arr[i][i].append(0) # trivial

    def idx(process, solver):
        return process*(df["s1"].max()+1) + solver

    for i, row in df.iterrows():
        ratio = 0
        if row["amount"] > 0:
            ratio = row["dup"]/row["amount"]
        arr[idx(row["p1"], row["s1"])][idx(row["p2"], row["s2"])].append(ratio)
    
    median_arr = np.zeros((n,n))

    for i in range(n):
        for j in range(n):
            median_arr[i][j] = statistics.median(arr[i][j])

    fig, ax1 = plt.subplots(figsize=(14, 14))
    im = ax1.imshow(median_arr)

    labels = ["(%s, %s)" % (p, s) for p in range(df["p1"].max()+1) for s in range(df["s1"].max()+1)]
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

    ax1.set_title("pairwise duplicate hashes in the sets of unique hashes (ratios)")
    fig.tight_layout()
    plt.show()
    plt.savefig(out_file)


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python3 % <plot> <outfile> <data-file>" % sys.argv[0])
    if len(sys.argv) == 4:
        if sys.argv[1] == "--plot-boxplot-dup-ratios":
            plot_boxplot_dup_ratios(sys.argv[2], sys.argv[3])
        elif sys.argv[1] == "--plot-prob-dup-ratios":
            plot_prob_dup_ratios(sys.argv[2], sys.argv[3])
        elif sys.argv[1] == "--plot-heatmap-dup-hash-ratios":
            plot_heatmap_dup_hash_ratios(sys.argv[2], sys.argv[3])
        elif sys.argv[1] == "--plot-time-dup":
            plot_time_dup(sys.argv[2], sys.argv[3])
        elif sys.argv[1] == "--plot-time-until-dup":
            plot_time_until_dup(sys.argv[2], sys.argv[3])
        else:
            print("No plot specified!")
