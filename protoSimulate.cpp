#include <bits/stdc++.h>
using namespace std;

/*
Analytical throughput curves for random access protocols.

Formulas used (a = propagation/tx time, G = attempt rate):

1) Slotted ALOHA:
   S = G * e^{-G}

2) Pure ALOHA:
   S = G * e^{-2G}

3) CSMA non-persistent:
   S = [ G * e^{-aG} ] / [ G*(1 + 2a) + e^{-aG} ]

4) CSMA 1-persistent:
   S = [ G * (1+G) * e^{-aG} ] / [ G*(1 + 2a) + (1+G)*e^{-aG} ]

5) CSMA p-persistent (unslotted approximation commonly shown in texts):
   S = [ G * p * e^{-aG} ] /
       { [1 - (1 - p) * e^{-aG}] * [ G*(1 + 2a) + p * e^{-aG} ] }

6) CSMA/CD (1-persistent with collision detection, classic analytic curve is close to 1-persistent CSMA under abort-on-collision):
   Here we plot the same analytic form as 1-persistent CSMA as a reference curve.
*/

struct Curve {
    string name;
    vector<double> S;
    double Smax = 0.0;
    double G_at_Smax = 0.0;
};

int main(int argc, char** argv) {
    // Parameters (can be overridden via CLI):
    // argv[1] = a (propagation ratio), argv[2] = G_max, argv[3] = G_step
    double a = 0.01;
    double G_max = 5.0;
    double dG = 0.01;

    if (argc >= 2) a = atof(argv[1]);
    if (argc >= 3) G_max = atof(argv[2]);
    if (argc >= 4) dG = atof(argv[3]);

    if (a < 0) a = 0;
    if (G_max <= 0) G_max = 5.0;
    if (dG <= 0) dG = 0.01;

    // p values for p-persistent CSMA
    const vector<double> pvals = {0.01, 0.1, 0.5, 1.0};

    // Prepare G vector
    vector<double> G;
    for (double g = 0.0; g <= G_max + 1e-12; g += dG) G.push_back(g);

    // Curves
    Curve c_slotted{"Slotted ALOHA"};
    Curve c_pure{"Pure ALOHA"};
    Curve c_csma_non{"CSMA non-persistent"};
    Curve c_csma_1p{"CSMA 1-persistent"};
    vector<Curve> c_csma_p;
    for (double p : pvals) {
        Curve c; 
        {
            ostringstream oss; 
            oss << "CSMA p-persistent (p=" << p << ")";
            c.name = oss.str();
        }
        c_csma_p.push_back(move(c));
    }
    Curve c_csmacd{"CSMA/CD (1-persistent reference)"};

    // Helpers to update maxima
    auto update_max = [](Curve& c, double Gval, double Sval) {
        if (Sval > c.Smax) {
            c.Smax = Sval;
            c.G_at_Smax = Gval;
        }
    };

    // Compute all curves
    for (double g : G) {
        double e_g   = exp(-g);
        double e_ag  = exp(-a * g);
        double e_2g  = exp(-2.0 * g);

        // 1) Slotted ALOHA
        double S_slot = g * e_g;
        c_slotted.S.push_back(S_slot);
        update_max(c_slotted, g, S_slot);

        // 2) Pure ALOHA
        double S_pure = g * e_2g;
        c_pure.S.push_back(S_pure);
        update_max(c_pure, g, S_pure);

        // 3) CSMA non-persistent
        {
            double num = g * e_ag;
            double den = g * (1.0 + 2.0 * a) + e_ag;
            double S = (den > 0.0) ? (num / den) : 0.0;
            c_csma_non.S.push_back(S);
            update_max(c_csma_non, g, S);
        }

        // 4) CSMA 1-persistent
        {
            double num = g * (1.0 + g) * e_ag;
            double den = g * (1.0 + 2.0 * a) + (1.0 + g) * e_ag;
            double S = (den > 0.0) ? (num / den) : 0.0;
            c_csma_1p.S.push_back(S);
            update_max(c_csma_1p, g, S);
        }

        // 5) CSMA p-persistent (for each p)
        for (size_t i = 0; i < pvals.size(); ++i) {
            double p = pvals[i];
            double num = g * p * e_ag;
            double denom_left = 1.0 - (1.0 - p) * e_ag;
            double denom_right = g * (1.0 + 2.0 * a) + p * e_ag;
            double den = denom_left * denom_right;
            double S = (den > 1e-15) ? (num / den) : 0.0;
            c_csma_p[i].S.push_back(S);
            update_max(c_csma_p[i], g, S);
        }

        // 6) CSMA/CD (reference curve ~ 1-persistent CSMA analytic form)
        {
            double num = g * (1.0 + g) * e_ag;
            double den = g * (1.0 + 2.0 * a) + (1.0 + g) * e_ag;
            double S = (den > 0.0) ? (num / den) : 0.0;
            c_csmacd.S.push_back(S);
            update_max(c_csmacd, g, S);
        }
    }

    // Write CSV
    const string outname = "throughput_curves.csv";
    ofstream out(outname);
    if (!out) {
        cerr << "Error: cannot open output file: " << outname << "\n";
        return 1;
    }
    out << fixed << setprecision(6);
    out << "G,S_slotted,S_pure,S_csma_non,S_csma_1p";
    for (double p : pvals) out << ",S_csma_p_" << p;
    out << ",S_csmacd\n";

    for (size_t i = 0; i < G.size(); ++i) {
        out << G[i]
            << "," << c_slotted.S[i]
            << "," << c_pure.S[i]
            << "," << c_csma_non.S[i]
            << "," << c_csma_1p.S[i];

        for (size_t k = 0; k < pvals.size(); ++k) out << "," << c_csma_p[k].S[i];

        out << "," << c_csmacd.S[i] << "\n";
    }
    out.close();

    // Print maxima summary
    auto print_max = [](const Curve& c){
        cout << setw(28) << left << c.name
             << " : S_max = " << setprecision(6) << c.Smax
             << " at G = " << setprecision(6) << c.G_at_Smax << "\n";
    };

    cout << "\n=== Parameters ===\n";
    cout << "a (prop delay ratio) = " << a << "\n";
    cout << "G in [0, " << G_max << "] step " << dG << "\n";
    cout << "Output file: " << outname << "\n\n";

    cout << "=== Max Throughput (Analytical) ===\n";
    print_max(c_slotted);
    print_max(c_pure);
    print_max(c_csma_non);
    print_max(c_csma_1p);
    for (const auto& c : c_csma_p) print_max(c);
    print_max(c_csmacd);

    return 0;
}
