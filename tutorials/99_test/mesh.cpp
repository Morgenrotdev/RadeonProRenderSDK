#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

int main() {
    int jmax, kmax, lmax, idomein;    // nufile_binaryer of grid points in xi, eta and zeta direction
    double xl, yl, zl;                // horizontal, vertical, depth computational dommain
    double dnx, dny, dnz, dx, dy, dz; // 1/dnx = dx (grid spacing)
    double unit = 1.0;
    int index;
    int ibinary = 0;
    int ibc = 1;
    int reflength;

    // boundary nufile_binaryer
    double bcfluid = 0, bcnonslip = 1, bcmovingwall = 2, bcinflow = 3, bcoutflow = 4, bcdammy = -1;

    // parameters set
    // jmax = 201;
    // kmax = 101;
    // lmax = 1;
    xl = 6.0;
    yl = 3.0;
    zl = 3.0;
    dnx = 5.0;
    dny = 2.5;
    dnz = 2.5;
    dx = unit / dnx;
    dy = unit / dny;
    dz = unit / dnz;
    printf("Grid Space: dx = %lf, dy = %lf, dz = %lf\n", dx, dy, dz);
    jmax = dnx * xl + 1;
    kmax = dny * yl + 1;
    lmax = dnz * zl + 1;

    idomein = jmax * kmax * lmax;
    vector<double> x(idomein, 0.0);
    vector<double> y(idomein, 0.0);
    vector<double> z(idomein, 0.0);
    vector<double> bc(idomein, 10);
    printf("Nufile_binaryer of Grid Poits: jmax = %d, kmax = %d, lmax = %d, total = %d\n", jmax, kmax, lmax, idomein);
     printf("reference length %d \n", reflength=xl/dx);

    for (int l = 0; l < lmax; l++) {
        for (int k = 0; k < kmax; k++) {
            for (int j = 0; j < jmax; j++) {
                x[j + k * jmax + l * jmax * kmax] = dx * double(j);
                y[j + k * jmax + l * jmax * kmax] = dy * double(k);
                z[j + k * jmax + l * jmax * kmax] = dz * double(l);
                // boundary condition and fluid region
                if (k == 0) {
                    bc[j + k * jmax + l * jmax * kmax] = bcnonslip;
                } else if (j == jmax - 1)  {
                    bc[j + k * jmax + l * jmax * kmax] = bcoutflow;
                } else if (j == 0) {
                    bc[j + k * jmax + l * jmax * kmax] = bcinflow;
                } else {
                    bc[j + k * jmax + l * jmax * kmax] = bcfluid;
                }
            }
        }
    }

    // for (int l = 0, index = 0; l <= lmax - 1; l++) {
    //     for (int k = 0; k <= kmax - 1; k++) {
    //         for (int j = 0; j <= jmax - 1; j++, index++) {
    //             x[index] = dx * double(j);
    //             y[index] = dy * double(k);
    //             z[index] = dz * double(l);
    //             // boundary condition and fluid region
    //             if ((l == 0 && j == 0) || (l == 0 && j == jmax - 1) || (l == 0 && k == 0) || (l == 0 && k == kmax - 1)) {
    //                 bc[index] = bcdammy;
    //             } else if (l == 0 && j == 1) {
    //                 bc[index] = bcinflow;
    //             } else if ((l == 0 && j == jmax - 2) || (l == 0 && k == kmax - 2)) {
    //                 bc[index] = bcoutflow;
    //             } else if (l == 0 && k == 1) {
    //                 bc[index] = bcnonslip;
    //             } else {
    //                 bc[index] = bcfluid;
    //             }
    //         }
    //     }
    // }

    // if (ibinary == 0) {
    ofstream file("gridbc_ascii.dat");
    if (!file.is_open()) {
        cerr << "failed to open grid.dat" << endl;
        return -1;
    }
    // ascii
    file << "TITLE = \" mesh data \"" << endl;
    file << "VARIABLES = \"X\", \"Y\", \"Z\", \"BC\" " << endl;
    file << "ZONE T = \"GRID\", I=" << jmax << ", J=" << kmax << ", K=" << lmax << ", F=POINT" << endl;
    // for (int index = 0; index <= idomein; index++) {
    for (int l = 0, index = 0; l <= lmax - 1; l++) {
        for (int k = 0; k <= kmax - 1; k++) {
            for (int j = 0; j <= jmax - 1; j++, index++) {
                file << x[index] << " " << y[index] << " " << z[index] << " " << bc[index] << endl;
            }
        }
    }
    file.close();
    //}
    //} else {
    // binary
    ofstream file_gridbinary("grid_binary.plt", ios::out | ios::binary);
    if (!file_gridbinary.is_open()) {
        cerr << "failed to open grid.plt" << endl;
        return -1;
    }

    file_gridbinary.write(reinterpret_cast<char*>(&jmax), sizeof(jmax));
    file_gridbinary.write(reinterpret_cast<char*>(&kmax), sizeof(kmax));
    file_gridbinary.write(reinterpret_cast<char*>(&lmax), sizeof(lmax));
    file_gridbinary.write(reinterpret_cast<char*>(x.data()), x.size() * sizeof(double));
    file_gridbinary.write(reinterpret_cast<char*>(y.data()), y.size() * sizeof(double));
    file_gridbinary.write(reinterpret_cast<char*>(z.data()), z.size() * sizeof(double));
    // file_binary.write(reinterpret_cast<char*>(bc.data()), bc.size() * sizeof(int));
    file_gridbinary.close();

    ofstream file_funcbinary("func_binary.fun", ios::out | ios::binary);
    if (!file_funcbinary.is_open()) {
        cerr << "failed to open grid.plt" << endl;
        return -1;
    }

    file_funcbinary.write(reinterpret_cast<char*>(&jmax), sizeof(jmax));
    file_funcbinary.write(reinterpret_cast<char*>(&kmax), sizeof(kmax));
    file_funcbinary.write(reinterpret_cast<char*>(&lmax), sizeof(lmax));
    file_funcbinary.write(reinterpret_cast<char*>(&ibc), sizeof(ibc));
    // file_funcbinary.write(reinterpret_cast<char*>(x.data()), x.size() * sizeof(double));
    //  file_funcbinary.write(reinterpret_cast<char*>(y.data()), y.size() * sizeof(double));
    //  file_funcbinary.write(reinterpret_cast<char*>(z.data()), z.size() * sizeof(double));
    file_funcbinary.write(reinterpret_cast<char*>(bc.data()), bc.size() * sizeof(double));

    file_funcbinary.close();

    ofstream file_bcgridbinary("bcgrid_binary.plt", ios::out | ios::binary);
    if (!file_bcgridbinary.is_open()) {
        cerr << "failed to open grid.plt" << endl;
        return -1;
    }

    file_bcgridbinary.write(reinterpret_cast<char*>(&jmax), sizeof(jmax));
    file_bcgridbinary.write(reinterpret_cast<char*>(&kmax), sizeof(kmax));
    file_bcgridbinary.write(reinterpret_cast<char*>(&lmax), sizeof(lmax));
    file_bcgridbinary.write(reinterpret_cast<char*>(x.data()), x.size() * sizeof(double));
    file_bcgridbinary.write(reinterpret_cast<char*>(y.data()), y.size() * sizeof(double));
    file_bcgridbinary.write(reinterpret_cast<char*>(z.data()), z.size() * sizeof(double));
    file_bcgridbinary.write(reinterpret_cast<char*>(bc.data()), bc.size() * sizeof(double));
    file_bcgridbinary.close();
    //    }
}
