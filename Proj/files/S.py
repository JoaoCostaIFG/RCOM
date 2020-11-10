Tprop = 2819355e-9
F = 10968 * 8
L = 256 * 8

print(Tprop)


def S(t, f=F, l=L, Tpipino=0, fer=0):
    global Tprop
    Tprop2 = Tprop
    Tprop2 += Tpipino
    R = f / t
    Tf = l / R
    a = Tprop2 / Tf
    S = (1-fer) / (1 + 2*a)
    print("R: %.5f \tTf: %.5f\tTp: %.5f \tS: %.5f" % (R, Tf, Tprop2, S))


Bauds = [9600, 19200, 38400, 57600, 115200]
chungus = [1, 10, 100, 1000, 10000]
chungus = [ch * 8 for ch in chungus]
# ts = [12.23109, 6.11795, 3.06131, 2.04245, 1.02359]  # Variar baud
# ts = [15.11709, 7.11741, 4.08493, 2.55259, 1.43659]  # Variar baud com erros
# ts = [58.20041, 8.42766, 3.44801, 2.94857, 2.90310]  # Variar chungus
# ts = [1.02574, 1.85891, 3.00061, 6.45144, 12.271156]  # Aumentar tprop
# ts = [0.03026, 3.06077, 66.29077]  # Variar file
ts = [1.16461, 1.19231, 1.23536, 1.28378, 1.30724, 1.33135, 1.44835,
      1.44912, 1.49755, 1.59548, 2.04525, 2.04511]  # Variar FER
fer = [0.11111, 0.14286, 0.15789, 0.18644, 0.21311, 0.22581,
       0.27273, 0.28358, 0.30435, 0.34247, 0.48387, 0.48936]
files = [8, 10968, 239765]
files = [f * 8 for f in files]
tprops = [10, 100, 200, 500, 1000]
tprops = [t * 1e-6 for t in tprops]
for i in range(len(ts)):
    # S(ts[i])
    S(ts[i], F, L, 0, fer[i])
