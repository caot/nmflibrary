#define ARMA_64BIT_WORD
#include <armadillo>
#include <stdio.h>
#include <iostream>
#include <fstream>

using namespace arma;
using namespace std;

inline int sub2ind(int i, int j, int n) {
    return (((i) * (n)) + j);
}

void removeNonZeroRowsCols(sp_fmat &currentMatrix, uword fullRows, uword fullCols) {
    cout << "removeNonZeroRowsCols nnz b4::" << currentMatrix.n_nonzero << endl;
    // sp_fmat zeroCols = sum(currentMatrix);
    // for (int i = 0; i < zeroCols.n_cols; i++) {
    //     if (zeroCols(0,i) == 0) {
    //         currentMatrix(fullRows-1, i) = 1e-6;
    //     }
    // }
    // sp_fmat zeroRows = sum(currentMatrix, 1);
    // for (int i = 0; i < zeroRows.n_rows; i++) {
    //     if (zeroRows(i,0) == 0) {
    //         currentMatrix(i, fullCols-1) = 1e-6;
    //     }
    // }
    fvec temp(fullRows);
    temp.fill(1e-6);
    currentMatrix.col(fullCols - 1) = currentMatrix.col(fullCols - 1) + temp;
    frowvec temp1(fullCols);
    temp1.fill(1e-6);
    currentMatrix.row(fullRows - 1) = currentMatrix.row(fullRows - 1) + temp1;
    if (currentMatrix.n_rows != fullRows || currentMatrix.n_cols != fullCols) {
        cout << "i didnt do good job::" << currentMatrix.n_rows
             << "x"  << currentMatrix.n_cols \
             << "::fullRows::" << fullRows
             << "::fullCols::" << fullCols << endl;
    }
    cout << "removeNonZeroRowsCols nnz after::" << currentMatrix.n_nonzero << endl;
}

void writeMatrixMarket(char* file_path, sp_fmat &input) {
    ofstream fileStream;
    fileStream.open(file_path);
    cout << "currentMatrix::" << input.n_rows \
         << "x" << input.n_cols << "::nnz::" << input.n_nonzero << endl;
    sp_fmat::const_iterator it = input.begin();
    while (it != input.end()) {
        fileStream << it.row()  << " " << it.col() << " " << *it << endl;
        ++it;
    }
}
void splitandWrite(sp_fmat A, int numSplits, char *outputDir, char *suffixStr, int pr = 1, int pc = 1) {
    // #pragma omp parallel for
    if (pr == 1 && pc == 1) {
        unsigned int perSplit = (unsigned int)ceil((A.n_rows * 1.0) / numSplits);
        char numSplitStr[6];
        sprintf(numSplitStr, "%d", numSplits);
        int fileNameLen = strlen(outputDir) + strlen(suffixStr) + 2 * strlen(numSplitStr) + 2;
        unsigned int m = A.n_rows;
        #pragma omp parallel for
        for (int i = 0; i <= numSplits; i++) {
            uword beginIdx = i * perSplit;
            uword endIdx = (i + 1) * perSplit - 1;
            if (endIdx > m)
                endIdx = m - 1;
            if (beginIdx < endIdx) {
                char* outputFileName = (char *)malloc(fileNameLen * sizeof(char));
                sprintf(outputFileName, "%s%s_%d_%d", outputDir, suffixStr, numSplits, i);
                cout << "beginIdx=" << beginIdx << " endIdx=" << endIdx
                     << " fileName=" << outputFileName << endl;
                sp_fmat currentMatrix = A.rows(beginIdx, endIdx);
                removeNonZeroRowsCols(currentMatrix, perSplit, A.n_cols);
                writeMatrixMarket(outputFileName, currentMatrix);
                free(outputFileName);
            }
        }
    } else {
        unsigned int perRowSplit = (unsigned int)ceil((A.n_rows * 1.0) / pr);
        unsigned int perColSplit = (unsigned int)ceil((A.n_cols * 1.0) / pc);
        char numSplitStr[6];
        snprintf(numSplitStr, 6, "%d", numSplits);
        unsigned int m = A.n_rows;
        unsigned int n = A.n_cols;
        int fileNameLen = strlen(outputDir) + strlen(suffixStr)
                          + 2 * strlen(numSplitStr) + 2;
        #pragma omp parallel for
        for (int i = 0; i <= pr; i++) {
            uword beginRowIdx = i * perRowSplit;
            uword endRowIdx = (i + 1) * perRowSplit - 1;
            if (endRowIdx > m)
                endRowIdx = m - 1;
            if (beginRowIdx < endRowIdx) {
                sp_fmat currentRowMatrix = A.rows(beginRowIdx, endRowIdx);
                //#pragma omp parallel for
                for (int j = 0; j <= pc; j++) {
                    uword beginColIdx = j * perColSplit;
                    uword endColIdx = (j + 1) * perColSplit - 1;
                    if (endColIdx > n)
                        endColIdx = n - 1;
                    char* outputFileName = (char *)malloc(fileNameLen * sizeof(char));
                    int mpi_rank = sub2ind(i, j, pc);
                    sprintf(outputFileName, "%s_%d_%d", outputDir, numSplits, mpi_rank);
                    if (beginColIdx < endColIdx) {
                        cout << "beginRowIdx=" << beginRowIdx << " endRowIdx=" << endRowIdx
                             << "beginColIdx=" << beginColIdx << " endColIdx=" << endColIdx
                             << " fileName=" << outputFileName << endl;
                        sp_fmat currentMatrix = currentRowMatrix.cols(beginColIdx, endColIdx);
                        removeNonZeroRowsCols(currentMatrix, perRowSplit, perColSplit);
                        writeMatrixMarket(outputFileName, currentMatrix);
                        free(outputFileName);
                    }
                }
            }
        }
    }
}
void splitFile(char* inputFile, char* outputDir, int numSplits, int pr = 1, int pc = 1) {
    std::string strif = std::string(inputFile);
    sp_fmat A, At;
    char* rowStr = "rows";
    char* colStr = "cols";
    //LoadMatrixMarketFile<sp_fmat, fvec, uword>(strif, m, n, nnz, A, false);
    //cout << "LMM Output:m=" << m << " n=" << n << " nnz=" << nnz << endl;
    A.load(strif, coord_ascii);
    //cout << "input matrix A:" << A << endl;
    int roundRowSplit = A.n_rows / numSplits;
    int roundColSplit = A.n_cols / numSplits;
    A = A.rows(0, roundRowSplit * numSplits - 1);
    A = A.cols(0, roundColSplit * numSplits - 1);
    cout << "Adjusted : m=" << A.n_rows << " n=" << A.n_cols << " nnz=" << A.n_nonzero << endl;
    splitandWrite(A, numSplits, outputDir, rowStr, pr, pc);
    if (pr == 1 && pc == 1) {
        splitandWrite(A.t(), numSplits, outputDir, colStr);
    }
}
void randSplit(int m, int n, float density, int seed, int numSplits, char *outputDir) {
    arma_rng::set_seed(seed);
    sp_fmat A = sprandu<sp_fmat>(m, n, density);
    char *rowStr = "rows";
    char *colStr = "cols";
    splitandWrite(A, numSplits, outputDir, rowStr);
    splitandWrite(A.t(), numSplits, outputDir, colStr);
}
void spMatIteratorTest() {
    sp_fmat A(5, 5);
    A.eye();
    sp_fmat::const_iterator it = A.begin();
    while (it != A.end()) {
        cout << it.row() << "," << it.col() << "," << *it << endl;
        ++it;
    }
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        cout << "Usage 1 : SplitFiles inputmtxfile outputdirectory numsplits [pr=1] [pc=1]" << endl;
        cout << "Usage 2 : SplitFiles m n density seed numsplits outputdirectory" << endl;
    }
    if (argc == 4) {
        splitFile(argv[1], argv[2], atoi(argv[3]));
    }
    if (argc == 6) {
        int numSplits = atoi(argv[3]);
        int pr = atoi(argv[4]);
        int pc = atoi(argv[5]);
        if (pr * pc != numSplits) {
            cout << "pr *pc != numSplits. Quitting the program" << endl;
            return -1;
        }
        splitFile(argv[1], argv[2], atoi(argv[3]), pr, pc);
    }
    if (argc == 7) {
        int m = atoi(argv[1]);
        int n = atoi(argv[2]);
        float  density = atof(argv[3]);
        int seed = atoi(argv[4]);
        int numSplits = atoi(argv[5]);
        randSplit(m, n, density, seed, numSplits, argv[6]);
    }
    return 0;
}
