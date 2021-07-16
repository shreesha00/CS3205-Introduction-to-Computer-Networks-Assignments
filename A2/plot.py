from matplotlib import pyplot as plt
import matplotlib.offsetbox as offsetbox
import sys
import os

cw_updates = {}
params = {}
for i in range(1, 33) :
    cw_updates[i] = []
    params[i] = []
    with open('Output/plot_' + str(i), "r") as fp:
        for line in fp :
            cw_updates[i].append(float(line.split("\n")[0]))
    with open('Output/plot_' + str(i) + '_params', "r") as fp :
        for line in fp :
            params[i].append(float(line.split("\n")[0])) 

fig, axs = plt.subplots(16, 2)
fig.suptitle('Plots for 32 parameter combinations', fontsize = 25)
fig.set_size_inches(18, 80)
fig.subplots_adjust(top=0.95)
counter = 1
for i in range(16) :
    for j in range(2) :
        axs[i, j].plot(cw_updates[counter])
        axs[i, j].set_xlabel("Update Number", fontsize = 15)
        axs[i, j].set_ylabel("Congestion Window Size ($cwnd$) in kB", fontsize = 15)
        Ki, Km, Kn, Kf, Ps = params[counter]
        text = "$K_i = " + str(Ki) + "$\n" + "$K_m = " + str(Km) + "$\n" + "$K_n = " + str(Kn) + "$\n" + "$K_f = " + str(Kf) + "$\n" + "$P_s = " + str(Ps) + "$\n"
        ob = offsetbox.AnchoredText(text, loc=1)
        ob.patch.set_edgecolor('black')
        ob.patch.set_facecolor('grey')
        axs[i, j].add_artist(ob)
        counter += 1
fig.savefig('./Output/Plots.pdf')
