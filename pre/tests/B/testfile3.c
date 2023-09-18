/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

/*
 * For array declaration.
 */

// ==================== Decl

const int D1 = 2;
const int D2 = 4;

const int _kArrA[4] = {1, 2, 3, 4};
const int _kArrB[2] = {1, 2}, _kArrC[2] = {3, 4};
const int _kArrD[2][2 + 2] = {
        {1,     2,     3, 4},
        {4 + 1, 2 * 3, 7, 8}
};

int _arrA[4] = {1, 2, 3, 4};
int _arrB[2] = {1, 2}, _arrC[2] = {3, 4};
int _arrD[4 / 2][2 * 2 + 0] = {
        {5 / 2 - 1, 2, 3, 4},
        {5,         6, 7, 8}
};
int _arrE[3][3];

// ==================== FuncDef
void one_dimentional(int arr[])
{
    printf("1D arr[0]: %d\n", arr[0]);
    printf("1D arr[1]: %d\n", arr[1]);
}

void two_dimentional(int arr[][4])
{
    printf("2D arr[0][0]: %d\n", arr[0][0]);
    printf("2D arr[1][3]: %d\n", arr[D1 - 1][D2 - 1]);
}

// ==================== MainFuncDef
int main()
{
    int _ia1d1[3];
    int _ia1d2[3] = {1, 2, 3};
    int _ia1d3[3] = {1, 2, 3}, _ia1d4[3] = {1, 2, 3};

    int _ia1[4][4];
    int _ia2[2][2] = {{1, 2},
                      {3, 4}};
    int _ia3[2][2] = {{1, 2}, {3, 4}}, _ia4[2][2] = {{1, 2}, {3, 4}};

    printf("21371300\n");

    one_dimentional(_arrA); // 1, 2
    two_dimentional(_arrD); // 1, 8

    // arithmetics
    // 3 = 1 + 2 * 3 - 4
    _arrA[0] = _arrA[0] + _arrA[1] * _arrA[2] - _arrA[3];
    one_dimentional(_arrA); // 3, 2
    _arrB[3 / 2] = 99 - 33 * 1; // 66
    one_dimentional(_arrB); // 1, 66

    // 7 = 2 + 5
    _arrD[1][D2 - 1] = _arrD[0][_arrD[0][0]] + _arrD[1][0];
    one_dimentional(_arrD[0]);  // 1, 2
    two_dimentional(_arrD); // 1, 7

    return 0;
}
