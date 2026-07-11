/**
 * Random Forest Model for Heart Disease Detection
 * Automatically generated using m2cgen for Contiki-NG
*/

#ifndef HEART_DISEASE_MODEL_H
#define HEART_DISEASE_MODEL_H

// Standard dependency for generated code
#include <math.h>

// Starting point for generated code
#include <string.h>
void add_vectors(double *v1, double *v2, int size, double *result) {
    for(int i = 0; i < size; ++i)
        result[i] = v1[i] + v2[i];
}
void mul_vector_number(double *v1, double num, int size, double *result) {
    for(int i = 0; i < size; ++i)
        result[i] = v1[i] * num;
}
void score(double * input, double * output) {
    double var0[2];
    double var1[2];
    double var2[2];
    double var3[2];
    double var4[2];
    double var5[2];
    double var6[2];
    double var7[2];
    if (input[3] <= 133.5) {
        if (input[4] <= 0.5) {
            if (input[0] <= 56.5) {
                memcpy(var7, (double[]){0.28125, 0.71875}, 2 * sizeof(double));
            } else {
                memcpy(var7, (double[]){0.08333333333333333, 0.9166666666666666}, 2 * sizeof(double));
            }
        } else {
            if (input[0] <= 62.5) {
                memcpy(var7, (double[]){0.0, 1.0}, 2 * sizeof(double));
            } else {
                memcpy(var7, (double[]){0.13333333333333333, 0.8666666666666667}, 2 * sizeof(double));
            }
        }
    } else {
        if (input[1] <= 0.5) {
            if (input[3] <= 242.0) {
                if (input[2] <= 174.0) {
                    if (input[0] <= 54.5) {
                        memcpy(var7, (double[]){0.9795918367346939, 0.02040816326530612}, 2 * sizeof(double));
                    } else {
                        memcpy(var7, (double[]){0.7857142857142857, 0.21428571428571427}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var7, (double[]){0.0, 1.0}, 2 * sizeof(double));
                }
            } else {
                if (input[2] <= 142.5) {
                    if (input[3] <= 248.5) {
                        memcpy(var7, (double[]){0.3333333333333333, 0.6666666666666666}, 2 * sizeof(double));
                    } else {
                        memcpy(var7, (double[]){0.8571428571428571, 0.14285714285714285}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var7, (double[]){0.3125, 0.6875}, 2 * sizeof(double));
                }
            }
        } else {
            if (input[5] <= 131.5) {
                if (input[0] <= 59.5) {
                    if (input[2] <= 103.0) {
                        memcpy(var7, (double[]){1.0, 0.0}, 2 * sizeof(double));
                    } else {
                        memcpy(var7, (double[]){0.25, 0.75}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var7, (double[]){0.07272727272727272, 0.9272727272727272}, 2 * sizeof(double));
                }
            } else {
                if (input[5] <= 167.0) {
                    if (input[5] <= 165.5) {
                        memcpy(var7, (double[]){0.5841584158415841, 0.4158415841584158}, 2 * sizeof(double));
                    } else {
                        memcpy(var7, (double[]){0.0, 1.0}, 2 * sizeof(double));
                    }
                } else {
                    if (input[3] <= 213.0) {
                        memcpy(var7, (double[]){0.5555555555555556, 0.4444444444444444}, 2 * sizeof(double));
                    } else {
                        memcpy(var7, (double[]){0.8771929824561403, 0.12280701754385964}, 2 * sizeof(double));
                    }
                }
            }
        }
    }
    double var8[2];
    if (input[5] <= 130.5) {
        if (input[2] <= 179.0) {
            if (input[4] <= 0.5) {
                if (input[0] <= 58.5) {
                    if (input[5] <= 109.0) {
                        memcpy(var8, (double[]){0.16666666666666666, 0.8333333333333334}, 2 * sizeof(double));
                    } else {
                        memcpy(var8, (double[]){0.4, 0.6}, 2 * sizeof(double));
                    }
                } else {
                    if (input[0] <= 73.5) {
                        memcpy(var8, (double[]){0.12698412698412698, 0.873015873015873}, 2 * sizeof(double));
                    } else {
                        memcpy(var8, (double[]){0.5714285714285714, 0.42857142857142855}, 2 * sizeof(double));
                    }
                }
            } else {
                if (input[3] <= 177.0) {
                    memcpy(var8, (double[]){0.0, 1.0}, 2 * sizeof(double));
                } else {
                    memcpy(var8, (double[]){0.14705882352941177, 0.8529411764705882}, 2 * sizeof(double));
                }
            }
        } else {
            memcpy(var8, (double[]){0.875, 0.125}, 2 * sizeof(double));
        }
    } else {
        if (input[1] <= 0.5) {
            if (input[3] <= 70.5) {
                memcpy(var8, (double[]){0.0, 1.0}, 2 * sizeof(double));
            } else {
                if (input[2] <= 165.0) {
                    if (input[4] <= 0.5) {
                        memcpy(var8, (double[]){0.8791208791208791, 0.12087912087912088}, 2 * sizeof(double));
                    } else {
                        memcpy(var8, (double[]){0.6666666666666666, 0.3333333333333333}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var8, (double[]){0.375, 0.625}, 2 * sizeof(double));
                }
            }
        } else {
            if (input[5] <= 177.5) {
                if (input[0] <= 56.5) {
                    if (input[0] <= 50.5) {
                        memcpy(var8, (double[]){0.5495495495495496, 0.45045045045045046}, 2 * sizeof(double));
                    } else {
                        memcpy(var8, (double[]){0.76, 0.24}, 2 * sizeof(double));
                    }
                } else {
                    if (input[0] <= 60.5) {
                        memcpy(var8, (double[]){0.19642857142857142, 0.8035714285714286}, 2 * sizeof(double));
                    } else {
                        memcpy(var8, (double[]){0.40425531914893614, 0.5957446808510638}, 2 * sizeof(double));
                    }
                }
            } else {
                memcpy(var8, (double[]){0.8461538461538461, 0.15384615384615385}, 2 * sizeof(double));
            }
        }
    }
    add_vectors(var7, var8, 2, var6);
    double var9[2];
    if (input[3] <= 42.5) {
        if (input[5] <= 158.5) {
            if (input[5] <= 95.5) {
                memcpy(var9, (double[]){0.0, 1.0}, 2 * sizeof(double));
            } else {
                if (input[5] <= 98.0) {
                    memcpy(var9, (double[]){1.0, 0.0}, 2 * sizeof(double));
                } else {
                    if (input[5] <= 117.5) {
                        memcpy(var9, (double[]){0.023255813953488372, 0.9767441860465116}, 2 * sizeof(double));
                    } else {
                        memcpy(var9, (double[]){0.09090909090909091, 0.9090909090909091}, 2 * sizeof(double));
                    }
                }
            }
        } else {
            memcpy(var9, (double[]){1.0, 0.0}, 2 * sizeof(double));
        }
    } else {
        if (input[3] <= 245.5) {
            if (input[0] <= 46.5) {
                if (input[3] <= 196.5) {
                    memcpy(var9, (double[]){0.7666666666666667, 0.23333333333333334}, 2 * sizeof(double));
                } else {
                    if (input[2] <= 149.0) {
                        memcpy(var9, (double[]){0.9714285714285714, 0.02857142857142857}, 2 * sizeof(double));
                    } else {
                        memcpy(var9, (double[]){0.875, 0.125}, 2 * sizeof(double));
                    }
                }
            } else {
                if (input[1] <= 0.5) {
                    memcpy(var9, (double[]){0.7111111111111111, 0.28888888888888886}, 2 * sizeof(double));
                } else {
                    if (input[5] <= 132.5) {
                        memcpy(var9, (double[]){0.2638888888888889, 0.7361111111111112}, 2 * sizeof(double));
                    } else {
                        memcpy(var9, (double[]){0.631578947368421, 0.3684210526315789}, 2 * sizeof(double));
                    }
                }
            }
        } else {
            if (input[2] <= 134.5) {
                if (input[1] <= 0.5) {
                    memcpy(var9, (double[]){0.9047619047619048, 0.09523809523809523}, 2 * sizeof(double));
                } else {
                    if (input[4] <= 0.5) {
                        memcpy(var9, (double[]){0.41025641025641024, 0.5897435897435898}, 2 * sizeof(double));
                    } else {
                        memcpy(var9, (double[]){0.23076923076923078, 0.7692307692307693}, 2 * sizeof(double));
                    }
                }
            } else {
                if (input[5] <= 160.5) {
                    if (input[1] <= 0.5) {
                        memcpy(var9, (double[]){0.25, 0.75}, 2 * sizeof(double));
                    } else {
                        memcpy(var9, (double[]){0.09523809523809523, 0.9047619047619048}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var9, (double[]){0.75, 0.25}, 2 * sizeof(double));
                }
            }
        }
    }
    add_vectors(var6, var9, 2, var5);
    double var10[2];
    if (input[5] <= 130.5) {
        if (input[3] <= 140.0) {
            if (input[4] <= 0.5) {
                memcpy(var10, (double[]){0.12121212121212122, 0.8787878787878788}, 2 * sizeof(double));
            } else {
                memcpy(var10, (double[]){0.0, 1.0}, 2 * sizeof(double));
            }
        } else {
            if (input[0] <= 61.5) {
                if (input[1] <= 0.5) {
                    memcpy(var10, (double[]){0.7142857142857143, 0.2857142857142857}, 2 * sizeof(double));
                } else {
                    if (input[3] <= 215.5) {
                        memcpy(var10, (double[]){0.425531914893617, 0.574468085106383}, 2 * sizeof(double));
                    } else {
                        memcpy(var10, (double[]){0.17073170731707318, 0.8292682926829268}, 2 * sizeof(double));
                    }
                }
            } else {
                memcpy(var10, (double[]){0.1590909090909091, 0.8409090909090909}, 2 * sizeof(double));
            }
        }
    } else {
        if (input[3] <= 42.5) {
            memcpy(var10, (double[]){0.13513513513513514, 0.8648648648648649}, 2 * sizeof(double));
        } else {
            if (input[3] <= 211.5) {
                if (input[1] <= 0.5) {
                    memcpy(var10, (double[]){0.9354838709677419, 0.06451612903225806}, 2 * sizeof(double));
                } else {
                    if (input[3] <= 206.5) {
                        memcpy(var10, (double[]){0.696969696969697, 0.30303030303030304}, 2 * sizeof(double));
                    } else {
                        memcpy(var10, (double[]){1.0, 0.0}, 2 * sizeof(double));
                    }
                }
            } else {
                if (input[3] <= 213.5) {
                    memcpy(var10, (double[]){0.0, 1.0}, 2 * sizeof(double));
                } else {
                    if (input[2] <= 165.0) {
                        memcpy(var10, (double[]){0.6340579710144928, 0.36594202898550726}, 2 * sizeof(double));
                    } else {
                        memcpy(var10, (double[]){0.0, 1.0}, 2 * sizeof(double));
                    }
                }
            }
        }
    }
    add_vectors(var5, var10, 2, var4);
    double var11[2];
    if (input[5] <= 128.5) {
        if (input[3] <= 145.5) {
            if (input[0] <= 62.5) {
                if (input[5] <= 112.0) {
                    memcpy(var11, (double[]){0.08333333333333333, 0.9166666666666666}, 2 * sizeof(double));
                } else {
                    memcpy(var11, (double[]){0.0, 1.0}, 2 * sizeof(double));
                }
            } else {
                memcpy(var11, (double[]){0.2, 0.8}, 2 * sizeof(double));
            }
        } else {
            if (input[1] <= 0.5) {
                memcpy(var11, (double[]){0.8333333333333334, 0.16666666666666666}, 2 * sizeof(double));
            } else {
                if (input[0] <= 39.5) {
                    memcpy(var11, (double[]){1.0, 0.0}, 2 * sizeof(double));
                } else {
                    if (input[4] <= 0.5) {
                        memcpy(var11, (double[]){0.23076923076923078, 0.7692307692307693}, 2 * sizeof(double));
                    } else {
                        memcpy(var11, (double[]){0.03571428571428571, 0.9642857142857143}, 2 * sizeof(double));
                    }
                }
            }
        }
    } else {
        if (input[0] <= 56.5) {
            if (input[3] <= 42.5) {
                memcpy(var11, (double[]){0.18181818181818182, 0.8181818181818182}, 2 * sizeof(double));
            } else {
                if (input[5] <= 171.0) {
                    if (input[3] <= 439.0) {
                        memcpy(var11, (double[]){0.7433628318584071, 0.25663716814159293}, 2 * sizeof(double));
                    } else {
                        memcpy(var11, (double[]){0.0, 1.0}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var11, (double[]){0.9074074074074074, 0.09259259259259259}, 2 * sizeof(double));
                }
            }
        } else {
            if (input[2] <= 133.0) {
                memcpy(var11, (double[]){0.25396825396825395, 0.746031746031746}, 2 * sizeof(double));
            } else {
                if (input[3] <= 188.5) {
                    memcpy(var11, (double[]){0.08695652173913043, 0.9130434782608695}, 2 * sizeof(double));
                } else {
                    if (input[2] <= 152.5) {
                        memcpy(var11, (double[]){0.5283018867924528, 0.4716981132075472}, 2 * sizeof(double));
                    } else {
                        memcpy(var11, (double[]){0.8, 0.2}, 2 * sizeof(double));
                    }
                }
            }
        }
    }
    add_vectors(var4, var11, 2, var3);
    double var12[2];
    if (input[3] <= 121.5) {
        if (input[2] <= 86.0) {
            memcpy(var12, (double[]){1.0, 0.0}, 2 * sizeof(double));
        } else {
            if (input[2] <= 175.0) {
                if (input[4] <= 0.5) {
                    if (input[0] <= 62.5) {
                        memcpy(var12, (double[]){0.11764705882352941, 0.8823529411764706}, 2 * sizeof(double));
                    } else {
                        memcpy(var12, (double[]){0.30434782608695654, 0.6956521739130435}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var12, (double[]){0.0, 1.0}, 2 * sizeof(double));
                }
            } else {
                memcpy(var12, (double[]){0.5, 0.5}, 2 * sizeof(double));
            }
        }
    } else {
        if (input[1] <= 0.5) {
            if (input[0] <= 57.5) {
                if (input[5] <= 105.0) {
                    memcpy(var12, (double[]){0.6363636363636364, 0.36363636363636365}, 2 * sizeof(double));
                } else {
                    if (input[0] <= 33.5) {
                        memcpy(var12, (double[]){0.7, 0.3}, 2 * sizeof(double));
                    } else {
                        memcpy(var12, (double[]){0.9655172413793104, 0.034482758620689655}, 2 * sizeof(double));
                    }
                }
            } else {
                memcpy(var12, (double[]){0.725, 0.275}, 2 * sizeof(double));
            }
        } else {
            if (input[5] <= 134.5) {
                if (input[2] <= 175.0) {
                    if (input[0] <= 45.5) {
                        memcpy(var12, (double[]){0.47368421052631576, 0.5263157894736842}, 2 * sizeof(double));
                    } else {
                        memcpy(var12, (double[]){0.18666666666666668, 0.8133333333333334}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var12, (double[]){0.6666666666666666, 0.3333333333333333}, 2 * sizeof(double));
                }
            } else {
                if (input[2] <= 165.0) {
                    if (input[4] <= 0.5) {
                        memcpy(var12, (double[]){0.7142857142857143, 0.2857142857142857}, 2 * sizeof(double));
                    } else {
                        memcpy(var12, (double[]){0.5365853658536586, 0.4634146341463415}, 2 * sizeof(double));
                    }
                } else {
                    memcpy(var12, (double[]){0.0, 1.0}, 2 * sizeof(double));
                }
            }
        }
    }
    add_vectors(var3, var12, 2, var2);
    double var13[2];
    if (input[4] <= 0.5) {
        if (input[1] <= 0.5) {
            if (input[2] <= 139.0) {
                if (input[5] <= 105.0) {
                    memcpy(var13, (double[]){0.0, 1.0}, 2 * sizeof(double));
                } else {
                    if (input[3] <= 70.5) {
                        memcpy(var13, (double[]){0.0, 1.0}, 2 * sizeof(double));
                    } else {
                        memcpy(var13, (double[]){0.9340659340659341, 0.06593406593406594}, 2 * sizeof(double));
                    }
                }
            } else {
                memcpy(var13, (double[]){0.55, 0.45}, 2 * sizeof(double));
            }
        } else {
            if (input[3] <= 42.5) {
                if (input[2] <= 107.0) {
                    memcpy(var13, (double[]){1.0, 0.0}, 2 * sizeof(double));
                } else {
                    memcpy(var13, (double[]){0.1044776119402985, 0.8955223880597015}, 2 * sizeof(double));
                }
            } else {
                if (input[0] <= 57.5) {
                    if (input[5] <= 131.0) {
                        memcpy(var13, (double[]){0.4358974358974359, 0.5641025641025641}, 2 * sizeof(double));
                    } else {
                        memcpy(var13, (double[]){0.738562091503268, 0.26143790849673204}, 2 * sizeof(double));
                    }
                } else {
                    if (input[5] <= 154.5) {
                        memcpy(var13, (double[]){0.3783783783783784, 0.6216216216216216}, 2 * sizeof(double));
                    } else {
                        memcpy(var13, (double[]){0.05, 0.95}, 2 * sizeof(double));
                    }
                }
            }
        }
    } else {
        if (input[1] <= 0.5) {
            memcpy(var13, (double[]){0.3157894736842105, 0.6842105263157895}, 2 * sizeof(double));
        } else {
            if (input[3] <= 124.5) {
                memcpy(var13, (double[]){0.0, 1.0}, 2 * sizeof(double));
            } else {
                if (input[0] <= 53.5) {
                    memcpy(var13, (double[]){0.7391304347826086, 0.2608695652173913}, 2 * sizeof(double));
                } else {
                    memcpy(var13, (double[]){0.2765957446808511, 0.723404255319149}, 2 * sizeof(double));
                }
            }
        }
    }
    add_vectors(var2, var13, 2, var1);
    mul_vector_number(var1, 0.14285714285714285, 2, var0);
    memcpy(output, var0, 2 * sizeof(double));
}

// End of generated code

#endif // HEART_DISEASE_MODEL_H
