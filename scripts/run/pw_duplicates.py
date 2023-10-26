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

def extract_pw_inst(sorted_agg_cls_file: str):

    amount_unique_hashes = {} # unique hashes that were reported in s1 and/or s2
    amount_dup_hashes = {} # amount of hashes that were reported reported in s1 and s2

    df = get_data(sorted_agg_cls_file, header=["hash", "process", "solver"])

    def less(f1,f2):
        (p1, s1) = f1
        (p2, s2) = f2
        if p1 < p2 or (p1 == p2  and s1 < s2):
            return True
        return False
    
    def incr_solver_pairs(founders, amount_dup_hashes):
        for f1 in founders:
            for f2 in founders:
                if f1 != f2:
                    if (f1, f2) not in amount_dup_hashes.keys():
                        amount_dup_hashes[(f1, f2)] = 1
                    else:
                        amount_dup_hashes[(f1, f2)] += 1

    last_h = ""
    founders = set()
    for idx, row in df.iterrows():
        if last_h != row["hash"]:

            incr_solver_pairs(list(founders), amount_dup_hashes)

            last_h = row["hash"]
            founders = set()

        f = (int(row["process"]), int(row["solver"]))
        if f not in founders:
            founders.add(f)
            if f not in amount_unique_hashes.keys():
                amount_unique_hashes[f] = 1
            else:
                amount_unique_hashes[f] += 1

    incr_solver_pairs(list(founders), amount_dup_hashes)

    for ((p1, s1), (p2, s2)), dup_hashes in amount_dup_hashes.items():
        print("%s %s %s %s %s %s" % (p1, s1, p2, s2, dup_hashes, amount_unique_hashes[(p1, s1)]+amount_unique_hashes[(p2, s2)]-dup_hashes))


if __name__ == "__main__":
    if len(sys.argv) == 3:
        if sys.argv[1] == "--extract-pw-inst":
            extract_pw_inst(sys.argv[2])
    else:
        print("Usage: python %s (--extract-pw-inst <sorted_agg_cls_file> )" % sys.argv[0])
