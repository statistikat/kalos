// Item 21-H.0 golden replay table for the final-information producer,
// generated from validation/evidence/native/final-information (double JVM
// capture through the FinalInformationProbe observer; see
// capture-final-information.R). All doubles are raw IEEE bit patterns.
// Do not edit by hand; regenerate with tools/gen_final_information_golden.py.
//
// status: 0 available, 1 inverse-failed (both SymmetricMatrix.inverse
// branches threw), 2 final-point-rejected. Nodes are the deduplicated
// deviance evaluations of the stencil plus the base node (the point with
// fx_hex, or its THROW for rejected cases, injected here because the probe
// observes the stencil, not the tail's own base evaluation); is_throw marks
// the newval catch (value replaced by fx). est cases additionally satisfy
// the probe's self-check: a replay of the pinned tail reproduced the
// estimator's own Point/score/information/covariance bit for bit.
#ifndef KALOS_TESTS_JVM_FINAL_INFORMATION_H
#define KALOS_TESTS_JVM_FINAL_INFORMATION_H

#include <cstddef>
#include <cstdint>

namespace golden_fi {

struct Node {
    const std::uint64_t* key;  // dim raw-bit coordinates
    std::uint64_t value;       // raw bits, valid unless is_throw
    std::uint8_t is_throw;     // newval caught the failure and used fx
};

struct Case {
    const char* id;
    int status, est;
    int p, bp, q, bq, d, bd, freq;
    int dim, nobs, check_all;
    std::uint64_t ar_limit, ma_limit, epsilon;
    const std::uint64_t* point;
    const std::uint64_t* yd;   // differenced response (sweep tier)
    const std::uint64_t* eps;
    std::uint64_t fx;
    const std::uint64_t* grad;
    const std::uint64_t* hess;
    const std::uint64_t* score;
    const std::uint64_t* info;
    const std::uint64_t* asy;
    const std::uint64_t* diag;
    const Node* nodes;
    int nodes_len;
};

static const std::uint64_t k_point_e_m0101[2] = {0xbfe100fc85a3a2c2ULL, 0xbfeffe9f08450e28ULL};
static const std::uint64_t k_yd_e_m0101[107] = {0xc00755bebdf2e380ULL, 0xbff3887acca64b00ULL, 0x40056f1469743920ULL, 0x3ffc284528df1040ULL, 0x3ff7a90e97e07840ULL, 0xbfed550372cbf440ULL, 0xbfe45e9092af4f40ULL, 0xbffcacf1a91ceea0ULL, 0x40109434d6d036c8ULL, 0xc000688de70ef970ULL, 0x3ffff79ee3944140ULL, 0x400d4c3b7569c780ULL, 0x3ff73518d690bda0ULL, 0xc0116ae8c1e74f08ULL, 0x40054751dd337fd0ULL, 0xbffd0ab19ef41120ULL, 0xbff985aa6e166600ULL, 0x400ed8e3e80b8050ULL, 0x3fd621bc02a9f300ULL, 0xbffc164bcd8a44c0ULL, 0x3ff5b2a2f611dd80ULL, 0xbfe05d54883f5900ULL, 0x3ff9c05d8cfc9100ULL, 0xc007e3a64096bc60ULL, 0x3fd4bc1045b29280ULL, 0x40127c2ab787f358ULL, 0xc015b5b00049e700ULL, 0x3ff8940f13c6dec0ULL, 0x3fecf1921ba0b040ULL, 0xc00768ceab1fd970ULL, 0xc007f0070a3b1230ULL, 0x400e8d8a1f9ae250ULL, 0xc0052bb5ecaf10e0ULL, 0x3ff78fd1c12cb7a0ULL, 0xbfda7da36cd09c00ULL, 0x3f9a8025165e8800ULL, 0xbfb0a0b2423e6000ULL, 0xbfe412d004b6d980ULL, 0x3ffff51e81552420ULL, 0xbfa7d82983680000ULL, 0x400094046a52db60ULL, 0xbff04a7dcc80a580ULL, 0xbfe2b80b21786e80ULL, 0x3fe7b2343fdb8100ULL, 0xc0038237a2dee150ULL, 0xbfedb7d045475b40ULL, 0xbfd888e813450b00ULL, 0x3fd1f91b0493e980ULL, 0xbfed99325e9c9fc0ULL, 0x3ffd6766952f6540ULL, 0xc0150e90c09e1840ULL, 0x3fe4d632da692800ULL, 0x3fe4e4c79de4fc00ULL, 0xbfd7b3daae6c1780ULL, 0x400b10c6d2b89000ULL, 0xbff2f865192a0260ULL, 0x4010155e5cc2f768ULL, 0xc01092ec020cbc60ULL, 0x3ffe59fbd65ecd80ULL, 0x3ff537182c2ba680ULL, 0xbfe5123da2878680ULL, 0xbffa7799a2d6c7c0ULL, 0x401768e0fa6616d0ULL, 0xc006d35aa6cfa720ULL, 0xbfb036d4a13dba00ULL, 0x3ffe9f6faea42260ULL, 0xc00322a0586ec040ULL, 0x3fd9e21dadfa3080ULL, 0xbfff262ba4397280ULL, 0x400dc480d73c54e0ULL, 0xc0012167be789c20ULL, 0xbfe2c78ddfe45c00ULL, 0x3fd118199523af00ULL, 0xbfc89a70dde4ce00ULL, 0xbfdacdae82534f80ULL, 0x3fe685cb0f4b8040ULL, 0x3fd4e5c131e13400ULL, 0xbffb4f670db23d80ULL, 0x3fe5d817d18f6240ULL, 0xbfef09f528273700ULL, 0x40053a8e5ec19580ULL, 0xbff73ec8838659e0ULL, 0xbfa8602875997400ULL, 0xbfb28c00f9481600ULL, 0xbff24f058a6330e0ULL, 0xbfdc1ed57e037f80ULL, 0xbff93ea4faccb660ULL, 0x3fd232b7e8cd1280ULL, 0xc00b4736c57d1850ULL, 0x4012c04cb7e2d4a0ULL, 0xbfe8f3d90c96dd40ULL, 0xbfebfec63f5d8380ULL, 0xc0031a62dab8daf0ULL, 0x3ff25557eb0b7160ULL, 0xc0019bfc7b5bb210ULL, 0xbfd66522d7db9b80ULL, 0x401df00c3d75c080ULL, 0xbfec81e1128508c0ULL, 0x3fb15b3ca9574c00ULL, 0x3ff95c84dc4cba00ULL, 0xbfe556a405e7b700ULL, 0xc003f223efb126d0ULL, 0x3fe9133374aaa040ULL, 0x400ac793abcead80ULL, 0x3ff39990baab6d00ULL, 0xbf9ca566f33d2800ULL, 0x3ff829146f458d00ULL};
static const std::uint64_t k_eps_e_m0101[2] = {0x3e4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_e_m0101[2] = {0xbf7f280005cc08aeULL, 0x3fa28e800373e3baULL};
static const std::uint64_t k_hess_e_m0101[4] = {0x408946c440b3f76fULL, 0xbfb18440034259b6ULL, 0xbfb18440034259b6ULL, 0x406aed07c50288b1ULL};
static const std::uint64_t k_score_e_m0101[2] = {0x3f55ba2ed4db31faULL, 0xbf79e1aa1cd0da30ULL};
static const std::uint64_t k_info_e_m0101[4] = {0x4061a07c616a5c04ULL, 0xbf886e51522aea92ULL, 0xbf886e51522aea92ULL, 0x4042c6f531f31041ULL};
static const std::uint64_t k_asy_e_m0101[4] = {0x3f7d0be8a0f09d82ULL, 0x3ec2e56b309526faULL, 0x3ec2e56b309526faULL, 0x3f9b446374be2842ULL};
static const std::uint64_t k_diag_e_m0101[2] = {0x3f7d0be8a0f09d82ULL, 0x3f9b446374be2842ULL};
static const std::uint64_t k_nk_e_m0101_0[2] = {0xbfe100fc7da3a2c3ULL, 0xbfeffe9f08450e28ULL};
static const std::uint64_t k_nk_e_m0101_1[2] = {0xbfe100fc85a3a2c2ULL, 0xbfeffe9f00450e29ULL};
static const std::uint64_t k_nk_e_m0101_2[2] = {0xbfe0fffc85a3ba93ULL, 0xbfeffe9f08450e28ULL};
static const std::uint64_t k_nk_e_m0101_3[2] = {0xbfe101fc85a38af1ULL, 0xbfeffe9f08450e28ULL};
static const std::uint64_t k_nk_e_m0101_4[2] = {0xbfe100fc85a3a2c2ULL, 0xbfeffd9f084525f9ULL};
static const std::uint64_t k_nk_e_m0101_5[2] = {0xbfe100fc85a3a2c2ULL, 0xbfefff9f0844f657ULL};
static const std::uint64_t k_nk_e_m0101_6[2] = {0xbfe1007c85a3aeaaULL, 0xbfeffe1f08451a10ULL};
static const std::uint64_t k_nk_e_m0101_7[2] = {0xbfe1017c85a396daULL, 0xbfefff1f08450240ULL};
static const std::uint64_t k_nk_e_m0101_8[2] = {0xbfe1017c85a396daULL, 0xbfeffe1f08451a10ULL};
static const std::uint64_t k_nk_e_m0101_9[2] = {0xbfe1007c85a3aeaaULL, 0xbfefff1f08450240ULL};
static const std::uint64_t k_nk_e_m0101_10[2] = {0xbfe100fc85a3a2c2ULL, 0xbfeffe9f08450e28ULL};
static const Node k_nodes_e_m0101[11] = {
    {k_nk_e_m0101_0, 0x4072d2231cfaeab0ULL, 0},
    {k_nk_e_m0101_1, 0x4072d2231cfb1797ULL, 0},
    {k_nk_e_m0101_2, 0x4072d22322538db7ULL, 0},
    {k_nk_e_m0101_3, 0x4072d2232445b95bULL, 0},
    {k_nk_e_m0101_4, 0x4072d223234d82acULL, 0},
    {k_nk_e_m0101_5, 0x4072d2231a060340ULL, 0},
    {k_nk_e_m0101_6, 0x4072d22320d06bbdULL, 0},
    {k_nk_e_m0101_7, 0x4072d2231d2596f1ULL, 0},
    {k_nk_e_m0101_8, 0x4072d22321c96f94ULL, 0},
    {k_nk_e_m0101_9, 0x4072d2231c2cd92bULL, 0},
    {k_nk_e_m0101_10, 0x4072d2231cfaf27aULL, 0},
};
static const std::uint64_t k_point_e_m111[2] = {0x3fcdaceace5356c8ULL, 0xbfd17972f6e08499ULL};
static const std::uint64_t k_yd_e_m111[143] = {0xbfe7031c0d4d35c0ULL, 0x3fc98d5536474800ULL, 0xc00985086f3d6590ULL, 0xbff158cc6394baa0ULL, 0x3ffd147e423dc5c0ULL, 0xbff65ee7854e7060ULL, 0x40016d76ae690c70ULL, 0x3fce70198a43ad00ULL, 0xbffd4da0820fe760ULL, 0x3ff06097d4869140ULL, 0xbfdb0aa503ba7280ULL, 0xbfeb317d0febca80ULL, 0x400f661039d9e4c0ULL, 0xc0082f3b65c1e5a0ULL, 0x3ff3369f4f742400ULL, 0xbffc65bb168a0020ULL, 0xbfe4a3e31767e9c0ULL, 0xbfd5dacff3c1c000ULL, 0xbff0e43f55ac1a40ULL, 0x3ff0eba4928409e0ULL, 0xbffd3d0b2638b2c0ULL, 0x3fc57b905d773900ULL, 0x3ff2a0655c92a9c0ULL, 0xc00edf1b4a7b0ac0ULL, 0x3fed371a05ea92c0ULL, 0x3ff3fbca39ea6740ULL, 0x3fc0134136daa900ULL, 0xbff2db6f7ae9b060ULL, 0x400696d035a9cb70ULL, 0xbffd33497b4c0160ULL, 0x3ff1ddf73c47d1a0ULL, 0xc00053e7f18e7a10ULL, 0x3ff01c685ca84f60ULL, 0xbff2f7e297b149c0ULL, 0xbfb39d8c7eec9800ULL, 0x3ff8c67a3d3ea4c0ULL, 0xbfe5fb06bb251640ULL, 0x3fa60cdcf49c9000ULL, 0x3fe4a1fd2c526300ULL, 0xc00777b3d37aacc0ULL, 0x3fa793e4f8e82400ULL, 0x400341e7979e0230ULL, 0xc0026782fd5f4810ULL, 0x3fda2048585f7b00ULL, 0xbff412ff7a8749e0ULL, 0xbffbe2ffe6638a40ULL, 0x3ff73bf951fda320ULL, 0xbfc98edb0ec32100ULL, 0xc00389f8b1482340ULL, 0xbff9e5929a1f2500ULL, 0x4004f8ca328f3660ULL, 0xc00d5696ae37d9c0ULL, 0xbf90482083f0c800ULL, 0x3ffea2558fdde300ULL, 0xc005b1bbdbdc6ab0ULL, 0x4005c3629a4e6fc0ULL, 0x400de2849a5cf2a0ULL, 0xc01083aaa0dc9ef0ULL, 0xbffd4ceb1d42abe0ULL, 0x40122fb34f2dbdd8ULL, 0xbff5ed9e5bae4800ULL, 0x3ffa34b3c905fa00ULL, 0xbfeb14dddc84af00ULL, 0xc00256aee8d47880ULL, 0x3fe8c042d3fa4c80ULL, 0xc00470fd74241460ULL, 0x3ffbe32612ce1660ULL, 0xc00009efa7d2e700ULL, 0x40022adbdc56b880ULL, 0x3f9d4af27fa9d800ULL, 0xbffc0aa1050888a0ULL, 0x4009e3e3f55abb20ULL, 0xc011bffa5933fc00ULL, 0xbfc0d91548ee3c00ULL, 0xbfdd6a6498a89e80ULL, 0x3fb18386eae26c00ULL, 0xc000991d98a77818ULL, 0x3fefca3e94e4e560ULL, 0xbfdc7f30645fdd80ULL, 0x3fe5d106994e8440ULL, 0xc000d48ea18b38f8ULL, 0x400f8d4321a94c88ULL, 0x3fd5ae067f3b3d80ULL, 0xbfb98b9cc257fc00ULL, 0x3ffc64295f1b1720ULL, 0xbff7a1b40180d520ULL, 0x40001466bc3574a0ULL, 0xc00cb1f20d5b9fa0ULL, 0x3fe3a2efe39b3a80ULL, 0xbfc85e2beba59500ULL, 0xbff77e1c8d2233a0ULL, 0x400cf6ac96745ba0ULL, 0xbff5d4143cfbd640ULL, 0x3ff54622fec8baa0ULL, 0xbfab178a84351000ULL, 0xbfdd7de0e5df1f00ULL, 0xbfb488205127e800ULL, 0x3ff844581c66e800ULL, 0x3fd01765b9778400ULL, 0xbfee6f0f4cb9ab00ULL, 0x3fda6e1cb48b1e80ULL, 0xbff02fdff5a99fa0ULL, 0x400a28e11158b130ULL, 0xbfde208f9f8b3a00ULL, 0x3fd9f07add17ac00ULL, 0xbffe4c816e400b00ULL, 0x40023b3e3cd80060ULL, 0xbff5732568bbeb80ULL, 0xbfef2b8699bd3440ULL, 0xbfe01ce133216a00ULL, 0xc008577e8d84a970ULL, 0x40124435e45f6bd0ULL, 0xbff86fd306433180ULL, 0xbfd3fbe440f52a80ULL, 0xbfea7e330ec2a780ULL, 0xbfec155ec4d79d00ULL, 0x3fe4742faaf96240ULL, 0x3fd3c7628fbcfd80ULL, 0x3fd929b11d9d5480ULL, 0x3ffe7190db45fb40ULL, 0x3feb3b96f02a5480ULL, 0xbff4dc3759c7b200ULL, 0x4000a7d2a7a3dcc0ULL, 0xc004c7b6320e84a0ULL, 0x400382fbee19f9e0ULL, 0xbf634359ba39c000ULL, 0xbff52d2ecab50ca0ULL, 0xbff8619ba45483a0ULL, 0x3ff6cc3c0b717c00ULL, 0xbff1e5d54907c3c0ULL, 0x3ff344c815311960ULL, 0x3ff4c9721f1f5be0ULL, 0x3ff4064749d42e40ULL, 0xbff9b5b1ab849760ULL, 0xbfa02058593ff800ULL, 0xbff8565849c8a180ULL, 0xc00cbf7347caaad0ULL, 0x3ff2948a9aae40e0ULL, 0xbfc6f467bfa95200ULL, 0xbfe3271bf8753b40ULL, 0xbffc9f8edcca7480ULL, 0xbfdf5a1c6ddc3a80ULL, 0xbfe3d1697acb7480ULL};
static const std::uint64_t k_eps_e_m111[2] = {0xbe4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_e_m111[2] = {0x3f44e00003e2514eULL, 0x3f91c000034d77b9ULL};
static const std::uint64_t k_hess_e_m111[4] = {0x408960256ab8b04fULL, 0xc0866e53de2c6e70ULL, 0xc0866e53de2c6e70ULL, 0x408b584455167c95ULL};
static const std::uint64_t k_score_e_m111[2] = {0xbf1ec9956fcf5b36ULL, 0xbf6a2db4b7fb6dc1ULL};
static const std::uint64_t k_info_e_m111[4] = {0x4062b6695fb700b9ULL, 0xc0608a872bcec9ffULL, 0xc0608a872bcec9ffULL, 0x40642a29844fabfcULL};
static const std::uint64_t k_asy_e_m111[4] = {0x3f98e25689de32efULL, 0x3f9469989e7c5c05ULL, 0x3f9469989e7c5c05ULL, 0x3f971794cfbb935fULL};
static const std::uint64_t k_diag_e_m111[2] = {0x3f98e25689de32efULL, 0x3f971794cfbb935fULL};
static const std::uint64_t k_nk_e_m111_0[2] = {0x3fcdaceaae5356ceULL, 0xbfd17972f6e08499ULL};
static const std::uint64_t k_nk_e_m111_1[2] = {0x3fcdaceace5356c8ULL, 0xbfd17972e6e0849cULL};
static const std::uint64_t k_nk_e_m111_2[2] = {0x3fcdb0eace52f784ULL, 0xbfd17972f6e08499ULL};
static const std::uint64_t k_nk_e_m111_3[2] = {0x3fcda8eace53b60cULL, 0xbfd17972f6e08499ULL};
static const std::uint64_t k_nk_e_m111_4[2] = {0x3fcdaceace5356c8ULL, 0xbfd17772f6e0b43bULL};
static const std::uint64_t k_nk_e_m111_5[2] = {0x3fcdaceace5356c8ULL, 0xbfd17b72f6e054f7ULL};
static const std::uint64_t k_nk_e_m111_6[2] = {0x3fcdaeeace532726ULL, 0xbfd17872f6e09c6aULL};
static const std::uint64_t k_nk_e_m111_7[2] = {0x3fcdaaeace53866aULL, 0xbfd17a72f6e06cc8ULL};
static const std::uint64_t k_nk_e_m111_8[2] = {0x3fcdaaeace53866aULL, 0xbfd17872f6e09c6aULL};
static const std::uint64_t k_nk_e_m111_9[2] = {0x3fcdaeeace532726ULL, 0xbfd17a72f6e06cc8ULL};
static const std::uint64_t k_nk_e_m111_10[2] = {0x3fcdaceace5356c8ULL, 0xbfd17972f6e08499ULL};
static const Node k_nodes_e_m111[11] = {
    {k_nk_e_m111_0, 0x4077e6998b4b5c55ULL, 0},
    {k_nk_e_m111_1, 0x4077e6998b4b6ebcULL, 0},
    {k_nk_e_m111_2, 0x4077e69991b9bdd1ULL, 0},
    {k_nk_e_m111_3, 0x4077e699918d0edaULL, 0},
    {k_nk_e_m111_4, 0x4077e69994588c27ULL, 0},
    {k_nk_e_m111_5, 0x4077e6998fea4ff9ULL, 0},
    {k_nk_e_m111_6, 0x4077e6998cefe312ULL, 0},
    {k_nk_e_m111_7, 0x4077e6998aa24f2eULL, 0},
    {k_nk_e_m111_8, 0x4077e69992751df2ULL, 0},
    {k_nk_e_m111_9, 0x4077e69990543e3bULL, 0},
    {k_nk_e_m111_10, 0x4077e6998b4b5cfcULL, 0},
};
static const std::uint64_t k_point_e_q3[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27ab6511534abULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_yd_e_q3[91] = {0x3ff2093a56c20640ULL, 0xc001de94cd51a300ULL, 0x3fe81a2ebde2c580ULL, 0xc01412fcb957a450ULL, 0x3ff5ae6b340fc000ULL, 0x4018cfb1b4f8b480ULL, 0xbfb33be9f836d000ULL, 0xbffb7ca5640cf000ULL, 0xc017e52ee4ac4350ULL, 0xc000381733ff9400ULL, 0x3ff045a0c2a21e40ULL, 0x4005e4da7b5548a0ULL, 0x40102b96ffbfaa70ULL, 0xc00a425abe47c020ULL, 0x3fd204652988d100ULL, 0x40058411e120fca0ULL, 0xc00d74f0ae06d840ULL, 0x401ba76a2db92af0ULL, 0xbffee4815590a100ULL, 0xbffe6c0cb7c2e6c0ULL, 0x40015ea692b6ece0ULL, 0xc01c469ac64e75c0ULL, 0x401497d9765dbc90ULL, 0xc00e1717a8a12b60ULL, 0x4006d5e042169cc0ULL, 0xbfeda8442145d800ULL, 0xc00916366af49e80ULL, 0x4011fe8849ee8040ULL, 0xc005aca25e87a4e0ULL, 0x4013060662af3310ULL, 0xbff874765fecc500ULL, 0x3fd3338dad1e0b00ULL, 0x3fb66878d8675400ULL, 0xbff6c883c25c7500ULL, 0x3ffc409c70ad82c0ULL, 0xbfe1b8a7e979ea00ULL, 0xbfe43a9770cad280ULL, 0xbfa385c024253000ULL, 0xbfe885dd05c5cb00ULL, 0x40018fae80c79360ULL, 0xbfe739fae06e9400ULL, 0x3ff2d26dfb530500ULL, 0x4009e86bc16e22e0ULL, 0xbfe7dd33719ac100ULL, 0x3ff20184e66e5040ULL, 0xbffb0639a62b6440ULL, 0xbff5fcff814dbd40ULL, 0xc0121793117163f0ULL, 0xbfde2fa647906d00ULL, 0x4009fdebfa5b7740ULL, 0xc00b42adbb359360ULL, 0x4001fa81ceaff9e0ULL, 0xbff140c9680f0740ULL, 0xc01c74c1a7cb1460ULL, 0x3ff0c84b3124df00ULL, 0xbfe58566a9baf580ULL, 0x400025550d5712c0ULL, 0x4006a455a39c2f00ULL, 0x401080df2f260f40ULL, 0xc00097f7e91ce9c0ULL, 0x3fe7c31ae14e8f00ULL, 0x4004a046ddfbf0e0ULL, 0xc0123e891228cc60ULL, 0x401d608b0f92a750ULL, 0xc00bfdc57470e0c0ULL, 0xbfd2ea01ae2f9700ULL, 0x3fd27d26468aa000ULL, 0xc00f3fc78f7535c0ULL, 0x3ff9b68b82000580ULL, 0x3ffadf7b654d5e80ULL, 0xc0105bc6b7a60180ULL, 0x3ff126745c8bb580ULL, 0x400031ba659ad560ULL, 0xbffc4471f18a9a40ULL, 0x401559244686fa40ULL, 0xbff34baf99e82080ULL, 0x3ff38fb6be50ce80ULL, 0x3fec87fadbf29a00ULL, 0x3fb0232e6b58e800ULL, 0x3ff031c703ca6640ULL, 0xc01588bb4f754410ULL, 0xbff28588f0be6580ULL, 0xbfed0014e4ae7c80ULL, 0xbff8db13763586c0ULL, 0x3fffc5ebf05c3200ULL, 0xbffb849c4058e640ULL, 0x3fe9d88f3c7c8580ULL, 0x400646a73e310ac0ULL, 0x3ff1605ee941dec0ULL, 0x3ff3315313f9ac40ULL, 0xc005cd908ea745c0ULL};
static const std::uint64_t k_eps_e_q3[3] = {0xbe4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_e_q3[3] = {0xbfb7d000046e3c8eULL, 0x3fb6ad8004382f77ULL, 0xbfb7bd00046ab38cULL};
static const std::uint64_t k_hess_e_q3[9] = {0x4095e4053812b298ULL, 0xc06722c0244e0057ULL, 0xc034a6b883d7a8fbULL, 0xc06722c0244e0057ULL, 0x4091555253399e6bULL, 0xc0577d55645edafdULL, 0xc034a6b883d7a8fbULL, 0xc0577d55645edafdULL, 0x4090c2bd051e585eULL};
static const std::uint64_t k_score_e_q3[3] = {0x3f7d84e387484506ULL, 0xbf7c1cc573c11130ULL, 0x3f7d6d55e289029fULL};
static const std::uint64_t k_info_e_q3[9] = {0x405b2301f217eea8ULL, 0xc02cae1edad2a218ULL, 0xbff999ab04c29283ULL, 0xc02cae1edad2a218ULL, 0x40557cb67b0cc45cULL, 0xc01d1e69576e1953ULL, 0xbff999ab04c29283ULL, 0xc01d1e69576e1953ULL, 0x4054c700585bf516ULL};
static const std::uint64_t k_asy_e_q3[9] = {0x3f834f63d1f55d08ULL, 0x3f5a3883aa94ef92ULL, 0x3f3515472e904a1bULL, 0x3f5a3883aa94ef92ULL, 0x3f889006e5aaab99ULL, 0x3f51b776c3f804bfULL, 0x3f3515472e904a1bULL, 0x3f51b776c3f804bfULL, 0x3f88d95064f0fc49ULL};
static const std::uint64_t k_diag_e_q3[3] = {0x3f834f63d1f55d08ULL, 0x3f889006e5aaab99ULL, 0x3f88d95064f0fc49ULL};
static const std::uint64_t k_nk_e_q3_0[3] = {0x3fdbb39ea9fe1509ULL, 0xbfd27ab6511534abULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_1[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27ab6411534aeULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_2[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27ab6511534abULL, 0x3fa9f4ec3fb8a0c3ULL};
static const std::uint64_t k_nk_e_q3_3[3] = {0x3fdbb59eb9fde564ULL, 0xbfd27ab6511534abULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_4[3] = {0x3fdbb19eb9fe44a8ULL, 0xbfd27ab6511534abULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_5[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd278b65115644dULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_6[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27cb651150509ULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_7[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27ab6511534abULL, 0x3faa04ecbfb7239dULL};
static const std::uint64_t k_nk_e_q3_8[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27ab6511534abULL, 0x3fa9e4ecbfba1db9ULL};
static const std::uint64_t k_nk_e_q3_9[3] = {0x3fdbb49eb9fdfd35ULL, 0xbfd279b651154c7cULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_10[3] = {0x3fdbb29eb9fe2cd7ULL, 0xbfd27bb651151cdaULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_11[3] = {0x3fdbb29eb9fe2cd7ULL, 0xbfd279b651154c7cULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_12[3] = {0x3fdbb49eb9fdfd35ULL, 0xbfd27bb651151cdaULL, 0x3fa9f4ecbfb8a0abULL};
static const std::uint64_t k_nk_e_q3_13[3] = {0x3fdbb49eb9fdfd35ULL, 0xbfd27ab6511534abULL, 0x3fa9fcecbfb7e224ULL};
static const std::uint64_t k_nk_e_q3_14[3] = {0x3fdbb29eb9fe2cd7ULL, 0xbfd27ab6511534abULL, 0x3fa9ececbfb95f32ULL};
static const std::uint64_t k_nk_e_q3_15[3] = {0x3fdbb29eb9fe2cd7ULL, 0xbfd27ab6511534abULL, 0x3fa9fcecbfb7e224ULL};
static const std::uint64_t k_nk_e_q3_16[3] = {0x3fdbb49eb9fdfd35ULL, 0xbfd27ab6511534abULL, 0x3fa9ececbfb95f32ULL};
static const std::uint64_t k_nk_e_q3_17[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd279b651154c7cULL, 0x3fa9fcecbfb7e224ULL};
static const std::uint64_t k_nk_e_q3_18[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27bb651151cdaULL, 0x3fa9ececbfb95f32ULL};
static const std::uint64_t k_nk_e_q3_19[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27bb651151cdaULL, 0x3fa9fcecbfb7e224ULL};
static const std::uint64_t k_nk_e_q3_20[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd279b651154c7cULL, 0x3fa9ececbfb95f32ULL};
static const std::uint64_t k_nk_e_q3_21[3] = {0x3fdbb39eb9fe1506ULL, 0xbfd27ab6511534abULL, 0x3fa9f4ecbfb8a0abULL};
static const Node k_nodes_e_q3[22] = {
    {k_nk_e_q3_0, 0x4081bf3a7e06ff53ULL, 0},
    {k_nk_e_q3_1, 0x4081bf3a7e06fd0eULL, 0},
    {k_nk_e_q3_2, 0x4081bf3a7e06ff2dULL, 0},
    {k_nk_e_q3_3, 0x4081bf3a7d8c37b5ULL, 0},
    {k_nk_e_q3_4, 0x4081bf3a89736a4bULL, 0},
    {k_nk_e_q3_5, 0x4081bf3a88070948ULL, 0},
    {k_nk_e_q3_6, 0x4081bf3a7cb13f46ULL, 0},
    {k_nk_e_q3_7, 0x4081bf3a7c48c7b4ULL, 0},
    {k_nk_e_q3_8, 0x4081bf3a88263633ULL, 0},
    {k_nk_e_q3_9, 0x4081bf3a7ff98255ULL, 0},
    {k_nk_e_q3_10, 0x4081bf3a80423208ULL, 0},
    {k_nk_e_q3_11, 0x4081bf3a86a6339eULL, 0},
    {k_nk_e_q3_12, 0x4081bf3a7b07acc0ULL, 0},
    {k_nk_e_q3_13, 0x4081bf3a7a75bdb6ULL, 0},
    {k_nk_e_q3_14, 0x4081bf3a86581342ULL, 0},
    {k_nk_e_q3_15, 0x4081bf3a807df826ULL, 0},
    {k_nk_e_q3_16, 0x4081bf3a80792643ULL, 0},
    {k_nk_e_q3_17, 0x4081bf3a7fd75920ULL, 0},
    {k_nk_e_q3_18, 0x4081bf3a801b52deULL, 0},
    {k_nk_e_q3_19, 0x4081bf3a7a8a7388ULL, 0},
    {k_nk_e_q3_20, 0x4081bf3a86242321ULL, 0},
    {k_nk_e_q3_21, 0x4081bf3a7e06cfb3ULL, 0},
};
static const std::uint64_t k_point_e_m6[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_yd_e_m6[143] = {0x3fa6f198628fe280ULL, 0xbfb1d2736229b5b0ULL, 0x3f7c54051e899600ULL, 0x3fab4675ecf46a40ULL, 0xbf7ed9656c4b0600ULL, 0xbfa241cf3a1e4360ULL, 0x3f9047765d778fe0ULL, 0x3fa3d9bc0f466dd0ULL, 0x3f9645ee0c27f380ULL, 0xbf83b218b5cef300ULL, 0xbf76624d1a392a00ULL, 0xbf91fa6506d96f80ULL, 0xbfa199d0668b99e0ULL, 0x3f8a2ace29f4e700ULL, 0xbf84b678faaece00ULL, 0xbfb4261b15fb0df0ULL, 0x3f81027a9e468880ULL, 0x3fb1dd68e3177910ULL, 0x3faa6545ee383760ULL, 0xbfa577dff4e82940ULL, 0x3fa9eaaa3d748300ULL, 0x3fa167bcc5d92780ULL, 0xbf467cff65c8b800ULL, 0x3fa0a2aedf45eee0ULL, 0x3fc24cffbc332fd8ULL, 0xbfa1d981f65075e0ULL, 0x3f4dce0b22cda800ULL, 0x3fabb1a277d7cbc0ULL, 0xbfa1fd8d5e387720ULL, 0xbfb73de31c78afc0ULL, 0xbfc2cb5ad85cb688ULL, 0xbfa08ccec0d24d00ULL, 0xbfb16b5c45a1c100ULL, 0xbf8c7fb106f66100ULL, 0x3fabef2b117f4940ULL, 0x3faac68b8af23d80ULL, 0xbfaa5eee3f3349a0ULL, 0x3fbf60459ff5bbf0ULL, 0xbf9ff4959d6f8980ULL, 0xbfb42d6824c22ca0ULL, 0xbfaefaae7fee81e0ULL, 0xbf852c051be4c500ULL, 0x3fb30e74777c5cb0ULL, 0x3f7349e9a4e44200ULL, 0x3f6934388d03d200ULL, 0xbf9cce66bde98040ULL, 0xbf820ec1644cb300ULL, 0xbfa248700f2dc060ULL, 0xbf8b957f4ea34c00ULL, 0x3f6999ac923c2800ULL, 0x3f81bf3ea1926b00ULL, 0xbfb382d11dfabf70ULL, 0x3f92a6a88a75f000ULL, 0xbf8209de1017ba80ULL, 0x3f8ccb1ceea05500ULL, 0x3f9a2dfe86000f80ULL, 0x3fa8dd371e7ad0e0ULL, 0x3fb207263c9258d0ULL, 0xbfa0a6c438046700ULL, 0x3fa75dcc9f65eac0ULL, 0x3fa70a70465afbc0ULL, 0xbf9e917ce0c27080ULL, 0xbf941575fe648100ULL, 0x3fb762b385605920ULL, 0x3fabeb43ea5a1360ULL, 0xbf91e3e00ab77b40ULL, 0xbfa0ed6fb2898a20ULL, 0xbf9113429c6d4480ULL, 0x3f9a74d0a5485a00ULL, 0x3f721031646ccc00ULL, 0x3fb9594afa7aa3a0ULL, 0xbf5d819e158c6000ULL, 0xbfb03341a08bfaa0ULL, 0xbfb79908670993e0ULL, 0xbf9706dc47d78600ULL, 0xbfa75802486c0b40ULL, 0xbfac02989d9dd080ULL, 0xbf9723c0c44dff40ULL, 0x3f9ea066273212c0ULL, 0x3fa4eda46f2f2540ULL, 0xbf949d2c31794e80ULL, 0x3fb3d36d5c02c080ULL, 0xbfad8b74b4fa32c0ULL, 0x3f9958606ec41f80ULL, 0x3fa3d78be1c6ea40ULL, 0x3fb420250f826cc0ULL, 0x3f85b54a66e91400ULL, 0xbf4653de54d24000ULL, 0xbf97143c36fcb080ULL, 0xbf84980f085f4c00ULL, 0xbfa5c7c980c2d120ULL, 0xbf8442f5ca782700ULL, 0x3f652223a6bb1200ULL, 0xbfa512b331531000ULL, 0x3fba5f0fff269de0ULL, 0x3f923ca87d868180ULL, 0x3fa3be15b68d1640ULL, 0x3fbed5817548ab20ULL, 0x3f96c4ee327bf980ULL, 0x3fa5629b3d961080ULL, 0xbfbb0ce0eda97a00ULL, 0x3f8fcdd5ecad3500ULL, 0xbfa52012816ad400ULL, 0xbfaf62e20e746100ULL, 0x3f9a255a2b2f0100ULL, 0x3fb784569ae437c0ULL, 0x3fa79a2012cf94c0ULL, 0x3fb667d5dd297900ULL, 0x3fc232a1f4480eb0ULL, 0x3f83f06735b4a000ULL, 0x3fa8aaa28aa9ec40ULL, 0xbfc74f5db124c870ULL, 0xbfaba43262e57700ULL, 0xbfb71a1575e134a0ULL, 0x3fac1b96438b9100ULL, 0xbf7f6bffbdaa6200ULL, 0x3f885d7bd9cadc00ULL, 0xbfa38138fcbac380ULL, 0xbfb676a72532c680ULL, 0x3fb48b3c0b2706a0ULL, 0x3fa444d7c223f600ULL, 0xbfb22fc409e6c700ULL, 0xbfc54dfb6aaaa000ULL, 0x3fc916ca9c2e9090ULL, 0xbf94dc5911d47400ULL, 0x3fb4785e9d54cac0ULL, 0xbfb3c8b007b01e40ULL, 0x3fbb50ce9bff65a0ULL, 0x3f9697e0aadd6680ULL, 0xbf93e38ef19a5f00ULL, 0x3f9192f5c4b99a00ULL, 0xbfa87bc00cd51b00ULL, 0xbfa228a242f65680ULL, 0x3fb9df0bf967b800ULL, 0x3fc87043f5b379a0ULL, 0xbfd1b1b06a52cbf0ULL, 0x3fae96b9dfc06b80ULL, 0xbfa9a7529b801b80ULL, 0xbfbbc186ed4b1ac0ULL, 0xbfb86f963eddbd80ULL, 0xbf867d0a2c750f00ULL, 0x3faff5ec151d79c0ULL, 0x3fcdb18d886240a0ULL};
static const std::uint64_t k_eps_e_m6[6] = {0x3e5ba261b9f3ede5ULL, 0xbe4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL, 0x3e59b12bb27aedaeULL, 0xbe4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_e_m6[6] = {0xbf494bb39128b52eULL, 0x3f17fac0047630d3ULL, 0xbf10f38003276afbULL, 0x3f1021d58982e998ULL, 0xbf30483003078b07ULL, 0x3f19edc004d3092eULL};
static const std::uint64_t k_hess_e_m6[36] = {0x405c0efe5226ad8cULL, 0x40582361fca7d9ffULL, 0x3fdadffa0f984a45ULL, 0xc02ce00ac1fe151dULL, 0xc02424b105ca26e7ULL, 0xbfc3255151219695ULL, 0x40582361fca7d9ffULL, 0x40696b9a1f9ed1faULL, 0xbff20b202c5b721bULL, 0xc031b0aaec1f9f3eULL, 0xc037b5d3bf495de4ULL, 0x401616ad1c9c1f6eULL, 0x3fdadffa0f984a45ULL, 0xbff20b202c5b721bULL, 0x3fe78ec8ea621a3aULL, 0xbfd156dee61b27f7ULL, 0x3fbcb4ab85574fb6ULL, 0xbfe385f343a1ee45ULL, 0xc02ce00ac1fe151dULL, 0xc031b0aaec1f9f3eULL, 0xbfd156dee61b27f7ULL, 0x402a3f9e56f02e97ULL, 0x4025759dc9c78771ULL, 0x3fce21d263e88861ULL, 0xc02424b105ca26e7ULL, 0xc037b5d3bf495de4ULL, 0x3fbcb4ab85574fb6ULL, 0x4025759dc9c78771ULL, 0x40262e4d278084beULL, 0xbfd03d4d93058494ULL, 0xbfc3255151219695ULL, 0x401616ad1c9c1f6eULL, 0xbfe385f343a1ee45ULL, 0x3fce21d263e88861ULL, 0xbfd03d4d93058494ULL, 0x3ff3c17d7dad0247ULL};
static const std::uint64_t k_score_e_m6[6] = {0x3fbebe06a4f83da5ULL, 0xbf8d24860074bd60ULL, 0x3f8499dd8da97112ULL, 0xbf839b0e15c835c5ULL, 0x3fa3c9aaa35998fbULL, 0xbf8f82f760b5f184ULL};
static const std::uint64_t k_info_e_m6[36] = {0x40d10cce0464badbULL, 0x40cd55e7af8340b8ULL, 0x405054ac99362223ULL, 0xc0a18bd5b7d1c1f4ULL, 0xc0987b02f7345331ULL, 0xc03744a6cce438aeULL, 0x40cd55e7af8340b8ULL, 0x40dee4cb981d2ddaULL, 0xc065edb2f4b906e0ULL, 0xc0a57fc391e18083ULL, 0xc0acd0c2a1a12890ULL, 0x408ad838774b67daULL, 0x405054ac99362223ULL, 0xc065edb2f4b906e0ULL, 0x405ca14fbfb48a57ULL, 0xc04512a1d93cce54ULL, 0x4031717abec8fc82ULL, 0xc057ba173b8ccd9dULL, 0xc0a18bd5b7d1c1f4ULL, 0xc0a57fc391e18083ULL, 0xc04512a1d93cce54ULL, 0x409fe6765e3978c2ULL, 0x409a147b5d11a4e7ULL, 0x40424f5df201958bULL, 0xc0987b02f7345331ULL, 0xc0acd0c2a1a12890ULL, 0x4031717abec8fc82ULL, 0x409a147b5d11a4e7ULL, 0x409af4eec0485ed2ULL, 0xc043bc703c253494ULL, 0xc03744a6cce438aeULL, 0x408ad838774b67daULL, 0xc057ba173b8ccd9dULL, 0x40424f5df201958bULL, 0xc043bc703c253494ULL, 0x406802735c3a83baULL};
static const std::uint64_t k_asy_e_m6[36] = {0x3f2581fb04aaf9baULL, 0xbf1d2f050ce2907eULL, 0x3f3bf6a6836104f7ULL, 0x3f3e198ed3276e30ULL, 0xbf4137d7173e138fULL, 0x3f4177929f3ab091ULL, 0xbf1d2f050ce2907eULL, 0x3f20dcf5df0af602ULL, 0xbf40a684e7784aacULL, 0xbf39b65e9a12905aULL, 0x3f41d10c8bb41dc2ULL, 0xbf456938f8845f30ULL, 0x3f3bf6a6836104f7ULL, 0xbf40a684e7784aacULL, 0x3f917aa59ac9fe96ULL, 0x3f63588b270544b8ULL, 0xbf68094e74a6af7fULL, 0x3f83e057c991ad22ULL, 0x3f3e198ed3276e30ULL, 0xbf39b65e9a12905aULL, 0x3f63588b270544b8ULL, 0x3f70430cd2308c8cULL, 0xbf716e9298ec1062ULL, 0x3f560c23fc3ac6f5ULL, 0xbf4137d7173e138fULL, 0x3f41d10c8bb41dc2ULL, 0xbf68094e74a6af7fULL, 0xbf716e9298ec1062ULL, 0x3f75fa1d169c0b4aULL, 0xbf60a392802a9edaULL, 0x3f4177929f3ab091ULL, 0xbf456938f8845f30ULL, 0x3f83e057c991ad22ULL, 0x3f560c23fc3ac6f5ULL, 0xbf60a392802a9edaULL, 0x3f89386aa73ed57eULL};
static const std::uint64_t k_diag_e_m6[6] = {0x3f2581fb04aaf9baULL, 0x3f20dcf5df0af602ULL, 0x3f917aa59ac9fe96ULL, 0x3f70430cd2308c8cULL, 0x3f75fa1d169c0b4aULL, 0x3f89386aa73ed57eULL};
static const std::uint64_t k_nk_e_m6_0[6] = {0xbffba261b82f9c48ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_1[6] = {0xbffba261bf1834b6ULL, 0x3fefc07adc85db4dULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_2[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b84ee955b67ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_3[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb0d6666cULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_4[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8f9cf4e48bULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_5[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22869548637ULL};
static const std::uint64_t k_nk_e_m6_6[6] = {0xbffba1b9871c45c9ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_7[6] = {0xbffba309f71423a3ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_8[6] = {0xbffba261bf1834b6ULL, 0x3fefc17ae485c37bULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_9[6] = {0xbffba261bf1834b6ULL, 0x3fefbf7ae485f31dULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_10[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50f850e94fc1dULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_11[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc507850e95baa5ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_12[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b089842f41ceULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_13[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b1cdea5620e4ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_14[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabc8fa4f4ccb9ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_15[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feaba8fa4f4fc5bULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_16[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a12871549e07ULL};
static const std::uint64_t k_nk_e_m6_17[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a32871546e65ULL};
static const std::uint64_t k_nk_e_m6_18[6] = {0xbffba20da31a3d3fULL, 0x3fefc0fae485cf64ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_19[6] = {0xbffba2b5db162c2dULL, 0x3fefbffae485e734ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_20[6] = {0xbffba2b5db162c2dULL, 0x3fefc0fae485cf64ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_21[6] = {0xbffba20da31a3d3fULL, 0x3fefbffae485e734ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_22[6] = {0xbffba20da31a3d3fULL, 0x3fefc07ae485db4cULL, 0x3fc50d850e952bbfULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_23[6] = {0xbffba2b5db162c2dULL, 0x3fefc07ae485db4cULL, 0x3fc509850e958b03ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_24[6] = {0xbffba2b5db162c2dULL, 0x3fefc07ae485db4cULL, 0x3fc50d850e952bbfULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_25[6] = {0xbffba20da31a3d3fULL, 0x3fefc07ae485db4cULL, 0x3fc509850e958b03ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_26[6] = {0xbffba261bf1834b6ULL, 0x3fefc0fae485cf64ULL, 0x3fc50d850e952bbfULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_27[6] = {0xbffba261bf1834b6ULL, 0x3fefbffae485e734ULL, 0x3fc509850e958b03ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_28[6] = {0xbffba261bf1834b6ULL, 0x3fefbffae485e734ULL, 0x3fc50d850e952bbfULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_29[6] = {0xbffba261bf1834b6ULL, 0x3fefc0fae485cf64ULL, 0x3fc509850e958b03ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_30[6] = {0xbffba20da31a3d3fULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b0da9db8f994ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_31[6] = {0xbffba2b5db162c2dULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b17cd0cc691eULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_32[6] = {0xbffba2b5db162c2dULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b0da9db8f994ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_33[6] = {0xbffba20da31a3d3fULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b17cd0cc691eULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_34[6] = {0xbffba261bf1834b6ULL, 0x3fefc0fae485cf64ULL, 0x3fc50b850e955b61ULL, 0xbff9b0da9db8f994ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_35[6] = {0xbffba261bf1834b6ULL, 0x3fefbffae485e734ULL, 0x3fc50b850e955b61ULL, 0xbff9b17cd0cc691eULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_36[6] = {0xbffba261bf1834b6ULL, 0x3fefbffae485e734ULL, 0x3fc50b850e955b61ULL, 0xbff9b0da9db8f994ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_37[6] = {0xbffba261bf1834b6ULL, 0x3fefc0fae485cf64ULL, 0x3fc50b850e955b61ULL, 0xbff9b17cd0cc691eULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_38[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50d850e952bbfULL, 0xbff9b0da9db8f994ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_39[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc509850e958b03ULL, 0xbff9b17cd0cc691eULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_40[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc509850e958b03ULL, 0xbff9b0da9db8f994ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_41[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50d850e952bbfULL, 0xbff9b17cd0cc691eULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_42[6] = {0xbffba20da31a3d3fULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_43[6] = {0xbffba2b5db162c2dULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_44[6] = {0xbffba2b5db162c2dULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_45[6] = {0xbffba20da31a3d3fULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_46[6] = {0xbffba261bf1834b6ULL, 0x3fefc0fae485cf64ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_47[6] = {0xbffba261bf1834b6ULL, 0x3fefbffae485e734ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_48[6] = {0xbffba261bf1834b6ULL, 0x3fefbffae485e734ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_49[6] = {0xbffba261bf1834b6ULL, 0x3fefc0fae485cf64ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_50[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50d850e952bbfULL, 0xbff9b12bb742b159ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_51[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc509850e958b03ULL, 0xbff9b12bb742b159ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_52[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc509850e958b03ULL, 0xbff9b12bb742b159ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_53[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50d850e952bbfULL, 0xbff9b12bb742b159ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_54[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b0da9db8f994ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_55[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b17cd0cc691eULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_56[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b17cd0cc691eULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_57[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b0da9db8f994ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a22871548636ULL};
static const std::uint64_t k_nk_e_m6_58[6] = {0xbffba20da31a3d3fULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_59[6] = {0xbffba2b5db162c2dULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_60[6] = {0xbffba2b5db162c2dULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_61[6] = {0xbffba20da31a3d3fULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_62[6] = {0xbffba261bf1834b6ULL, 0x3fefc0fae485cf64ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_63[6] = {0xbffba261bf1834b6ULL, 0x3fefbffae485e734ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_64[6] = {0xbffba261bf1834b6ULL, 0x3fefbffae485e734ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_65[6] = {0xbffba261bf1834b6ULL, 0x3fefc0fae485cf64ULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_66[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50d850e952bbfULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_67[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc509850e958b03ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_68[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc509850e958b03ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_69[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50d850e952bbfULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_70[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b0da9db8f994ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_71[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b17cd0cc691eULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_72[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b17cd0cc691eULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_73[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b0da9db8f994ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_74[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_75[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_76[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb0fa4f4f072ULL, 0xbfe6a1a87154921eULL};
static const std::uint64_t k_nk_e_m6_77[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabc0fa4f4d8a2ULL, 0xbfe6a2a871547a4eULL};
static const std::uint64_t k_nk_e_m6_78[6] = {0xbffba261bf1834b6ULL, 0x3fefc07ae485db4cULL, 0x3fc50b850e955b61ULL, 0xbff9b12bb742b159ULL, 0x3feabb8fa4f4e48aULL, 0xbfe6a22871548636ULL};
static const Node k_nodes_e_m6[79] = {
    {k_nk_e_m6_0, 0x3fdc2e9513663874ULL, 0},
    {k_nk_e_m6_1, 0x3fdc2e95136b4e98ULL, 0},
    {k_nk_e_m6_2, 0x3fdc2e95136bf251ULL, 0},
    {k_nk_e_m6_3, 0x3fdc2e95136c1621ULL, 0},
    {k_nk_e_m6_4, 0x3fdc2e95136cb306ULL, 0},
    {k_nk_e_m6_5, 0x3fdc2e95136c163aULL, 0},
    {k_nk_e_m6_6, 0x3fdc2e9a9cf7ce4eULL, 0},
    {k_nk_e_m6_7, 0x3fdc2e9ba763a560ULL, 0},
    {k_nk_e_m6_8, 0x3fdc2e9b837170c5ULL, 0},
    {k_nk_e_m6_9, 0x3fdc2e9b5932f9b3ULL, 0},
    {k_nk_e_m6_10, 0x3fdc2e9510d44bfcULL, 0},
    {k_nk_e_m6_11, 0x3fdc2e9521ca757dULL, 0},
    {k_nk_e_m6_12, 0x3fdc2e95c61f3a34ULL, 0},
    {k_nk_e_m6_13, 0x3fdc2e95b1e899ccULL, 0},
    {k_nk_e_m6_14, 0x3fdc2e954b9f8e34ULL, 0},
    {k_nk_e_m6_15, 0x3fdc2e958caa37edULL, 0},
    {k_nk_e_m6_16, 0x3fdc2e952a444845ULL, 0},
    {k_nk_e_m6_17, 0x3fdc2e951054923bULL, 0},
    {k_nk_e_m6_18, 0x3fdc2e99ee3d4313ULL, 0},
    {k_nk_e_m6_19, 0x3fdc2e9a64847bc6ULL, 0},
    {k_nk_e_m6_20, 0x3fdc2e967c0bec1eULL, 0},
    {k_nk_e_m6_21, 0x3fdc2e95e875feedULL, 0},
    {k_nk_e_m6_22, 0x3fdc2e9653f840deULL, 0},
    {k_nk_e_m6_23, 0x3fdc2e96e19c4e21ULL, 0},
    {k_nk_e_m6_24, 0x3fdc2e96d4c7543cULL, 0},
    {k_nk_e_m6_25, 0x3fdc2e9657f8cc6fULL, 0},
    {k_nk_e_m6_26, 0x3fdc2e96aa12ad12ULL, 0},
    {k_nk_e_m6_27, 0x3fdc2e96a41a371bULL, 0},
    {k_nk_e_m6_28, 0x3fdc2e96a4ad7e15ULL, 0},
    {k_nk_e_m6_29, 0x3fdc2e96bb8a8640ULL, 0},
    {k_nk_e_m6_30, 0x3fdc2e96239ab666ULL, 0},
    {k_nk_e_m6_31, 0x3fdc2e969e90a61dULL, 0},
    {k_nk_e_m6_32, 0x3fdc2e97691fbab5ULL, 0},
    {k_nk_e_m6_33, 0x3fdc2e96d9bdf392ULL, 0},
    {k_nk_e_m6_34, 0x3fdc2e9686fa88f0ULL, 0},
    {k_nk_e_m6_35, 0x3fdc2e966e3ceb56ULL, 0},
    {k_nk_e_m6_36, 0x3fdc2e972bc01299ULL, 0},
    {k_nk_e_m6_37, 0x3fdc2e973021d7f5ULL, 0},
    {k_nk_e_m6_38, 0x3fdc2e953e8356d0ULL, 0},
    {k_nk_e_m6_39, 0x3fdc2e953cd2e1beULL, 0},
    {k_nk_e_m6_40, 0x3fdc2e9549bd84b2ULL, 0},
    {k_nk_e_m6_41, 0x3fdc2e953716eb32ULL, 0},
    {k_nk_e_m6_42, 0x3fdc2e9625a7f72dULL, 0},
    {k_nk_e_m6_43, 0x3fdc2e96cb094556ULL, 0},
    {k_nk_e_m6_44, 0x3fdc2e9714cd3ef6ULL, 0},
    {k_nk_e_m6_45, 0x3fdc2e96afabbb84ULL, 0},
    {k_nk_e_m6_46, 0x3fdc2e96587bf1b1ULL, 0},
    {k_nk_e_m6_47, 0x3fdc2e966a72a7fcULL, 0},
    {k_nk_e_m6_48, 0x3fdc2e9707cc45ddULL, 0},
    {k_nk_e_m6_49, 0x3fdc2e97367f8f7dULL, 0},
    {k_nk_e_m6_50, 0x3fdc2e951701e684ULL, 0},
    {k_nk_e_m6_51, 0x3fdc2e9540098f44ULL, 0},
    {k_nk_e_m6_52, 0x3fdc2e951e972031ULL, 0},
    {k_nk_e_m6_53, 0x3fdc2e9536a90adfULL, 0},
    {k_nk_e_m6_54, 0x3fdc2e957ef2ed7cULL, 0},
    {k_nk_e_m6_55, 0x3fdc2e959552d493ULL, 0},
    {k_nk_e_m6_56, 0x3fdc2e9507fe1fa2ULL, 0},
    {k_nk_e_m6_57, 0x3fdc2e9532bc43efULL, 0},
    {k_nk_e_m6_58, 0x3fdc2e965c96ddafULL, 0},
    {k_nk_e_m6_59, 0x3fdc2e96d4ff6b7cULL, 0},
    {k_nk_e_m6_60, 0x3fdc2e96e362fa4dULL, 0},
    {k_nk_e_m6_61, 0x3fdc2e9651587a2dULL, 0},
    {k_nk_e_m6_62, 0x3fdc2e96d0579042ULL, 0},
    {k_nk_e_m6_63, 0x3fdc2e96b506c42aULL, 0},
    {k_nk_e_m6_64, 0x3fdc2e9695c23d63ULL, 0},
    {k_nk_e_m6_65, 0x3fdc2e96974162a7ULL, 0},
    {k_nk_e_m6_66, 0x3fdc2e95172a9fd2ULL, 0},
    {k_nk_e_m6_67, 0x3fdc2e9512ad78c4ULL, 0},
    {k_nk_e_m6_68, 0x3fdc2e9524872a8eULL, 0},
    {k_nk_e_m6_69, 0x3fdc2e950f13e7a8ULL, 0},
    {k_nk_e_m6_70, 0x3fdc2e954ccdeb60ULL, 0},
    {k_nk_e_m6_71, 0x3fdc2e9535a8dc31ULL, 0},
    {k_nk_e_m6_72, 0x3fdc2e95403f6e74ULL, 0},
    {k_nk_e_m6_73, 0x3fdc2e953d717e92ULL, 0},
    {k_nk_e_m6_74, 0x3fdc2e952143d7e5ULL, 0},
    {k_nk_e_m6_75, 0x3fdc2e9534d8d518ULL, 0},
    {k_nk_e_m6_76, 0x3fdc2e9543d8136dULL, 0},
    {k_nk_e_m6_77, 0x3fdc2e951653ecf3ULL, 0},
    {k_nk_e_m6_78, 0x3fdc2e95136bae83ULL, 0},
};
static const std::uint64_t k_point_e_short[2] = {0xbfe11fe99937ea45ULL, 0xbfeffbf189fad929ULL};
static const std::uint64_t k_yd_e_short[23] = {0x4015a7bae9e23978ULL, 0xc00b006d80250130ULL, 0x3fe792614babea80ULL, 0x3ff2939054ab79e0ULL, 0xbfd5565f287f7e00ULL, 0xbffbf64f38e83860ULL, 0x3feec6f012e11380ULL, 0xc00d9d46e74c6cc0ULL, 0x3ff48960076d8b40ULL, 0x3fd3f59cb5f28a80ULL, 0x3ff8b1403ea4dde0ULL, 0x3fd8617c3142cf80ULL, 0xc005b61e3eaaef50ULL, 0x400844df142a6840ULL, 0xbffc117e46723460ULL, 0xc00182e8527be7f0ULL, 0x4006147b6c416680ULL, 0x3fed7c4c854b3840ULL, 0xc0094d0045a17980ULL, 0x4011abb90c44d968ULL, 0xc00f126905a6a0e0ULL, 0xbff1687d441bcb00ULL, 0xc0055fe30b6f2710ULL};
static const std::uint64_t k_eps_e_short[2] = {0x3e4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_e_short[2] = {0xbef4000003b8a3beULL, 0x3f79dd0004cfeb59ULL};
static const std::uint64_t k_hess_e_short[4] = {0x4064482073c60f48ULL, 0x3f95b1c00409581eULL, 0x3f95b1c00409581eULL, 0x4029863044bfc458ULL};
static const std::uint64_t k_score_e_short[2] = {0x3ec440ed95579ba2ULL, 0xbf4a30f66f2c8a9dULL};
static const std::uint64_t k_info_e_short[4] = {0x403489f82c58f870ULL, 0x3f65f82db5ee77a3ULL, 0x3f65f82db5ee77a3ULL, 0x3ff9d90cdbecdf68ULL};
static const std::uint64_t k_asy_e_short[4] = {0x3fa8eda270d590fcULL, 0xbf1530161dbebac3ULL, 0xbf1530161dbebac3ULL, 0x3fe3cee7eb36ded4ULL};
static const std::uint64_t k_diag_e_short[2] = {0x3fa8eda270d590fcULL, 0x3fe3cee7eb36ded4ULL};
static const std::uint64_t k_nk_e_short_0[2] = {0xbfe11fe99137ea46ULL, 0xbfeffbf189fad929ULL};
static const std::uint64_t k_nk_e_short_1[2] = {0xbfe11fe99937ea45ULL, 0xbfeffbf181fad92aULL};
static const std::uint64_t k_nk_e_short_2[2] = {0xbfe11ee999380216ULL, 0xbfeffbf189fad929ULL};
static const std::uint64_t k_nk_e_short_3[2] = {0xbfe120e99937d274ULL, 0xbfeffbf189fad929ULL};
static const std::uint64_t k_nk_e_short_4[2] = {0xbfe11fe99937ea45ULL, 0xbfeffaf189faf0faULL};
static const std::uint64_t k_nk_e_short_5[2] = {0xbfe11fe99937ea45ULL, 0xbfeffcf189fac158ULL};
static const std::uint64_t k_nk_e_short_6[2] = {0xbfe11f699937f62dULL, 0xbfeffb7189fae511ULL};
static const std::uint64_t k_nk_e_short_7[2] = {0xbfe120699937de5dULL, 0xbfeffc7189facd41ULL};
static const std::uint64_t k_nk_e_short_8[2] = {0xbfe120699937de5dULL, 0xbfeffb7189fae511ULL};
static const std::uint64_t k_nk_e_short_9[2] = {0xbfe11f699937f62dULL, 0xbfeffc7189facd41ULL};
static const std::uint64_t k_nk_e_short_10[2] = {0xbfe11fe99937ea45ULL, 0xbfeffbf189fad929ULL};
static const Node k_nodes_e_short[11] = {
    {k_nk_e_short_0, 0x4054bcade71f6ec3ULL, 0},
    {k_nk_e_short_1, 0x4054bcade71f88b4ULL, 0},
    {k_nk_e_short_2, 0x4054bcadec2eb247ULL, 0},
    {k_nk_e_short_3, 0x4054bcadec343b9fULL, 0},
    {k_nk_e_short_4, 0x4054bcadeac13c28ULL, 0},
    {k_nk_e_short_5, 0x4054bcade449d308ULL, 0},
    {k_nk_e_short_6, 0x4054bcadea1a0b4eULL, 0},
    {k_nk_e_short_7, 0x4054bcade6e10e32ULL, 0},
    {k_nk_e_short_8, 0x4054bcadea1c94f9ULL, 0},
    {k_nk_e_short_9, 0x4054bcade6de2dc0ULL, 0},
    {k_nk_e_short_10, 0x4054bcade71f6ed7ULL, 0},
};
static const std::uint64_t k_point_e_qtr3[3] = {0x3fc546acea74e37fULL, 0x3fb8407bce018cb8ULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_yd_e_qtr3[91] = {0x40178bd18f993490ULL, 0x3ff42d98b3234100ULL, 0xbff20888e3774940ULL, 0x3ff1d1f44678a780ULL, 0xbff86d66a4286240ULL, 0x3fda394619a7fb00ULL, 0x401866d4919beaa0ULL, 0xbffa3cbaf7604fc0ULL, 0xc009d871a2265460ULL, 0xbff589db10952780ULL, 0xc01ea4517d94da90ULL, 0x400734ee35bb6240ULL, 0x3ff7827f77e1a200ULL, 0x3ffb9d09507d6dc0ULL, 0xbfe52e8de0859080ULL, 0xbff722e7ea0b5180ULL, 0x3ffe432e9db28c80ULL, 0xbfddbaa066bd9b00ULL, 0x3ff09d8011178b00ULL, 0xbfe2b2120a755600ULL, 0x3ff1fc64e0395f80ULL, 0x3fecb113c5800980ULL, 0x4012ee32b21db1e0ULL, 0xbfe5f4a8a518a000ULL, 0xc00eeb548d267c60ULL, 0xc00239477e900960ULL, 0xc00ae43b55c29440ULL, 0x4006477ca6114160ULL, 0xbff81b07084b2c40ULL, 0x3fef7af37cda6400ULL, 0x3ff364bf8c3d5e80ULL, 0xc0032b5639f79d40ULL, 0x40008a1db4e7a6c0ULL, 0x3ff8c2fa389c4800ULL, 0xc00a2e63287e9760ULL, 0x401610fde719afc0ULL, 0x40020ed736e611c0ULL, 0xc012f9f3b13c3a00ULL, 0x3ff7695c9bf4b0c0ULL, 0xc00018f63f7032c0ULL, 0xc01065b709ed8a80ULL, 0x401a8ab16a940760ULL, 0xbfe63b0d0eabe700ULL, 0xbfd137ceb2b24100ULL, 0x3ff89c95450330c0ULL, 0xc01a2f5ebc3e5d10ULL, 0x3ffc38a66f0ed140ULL, 0xc00486bfac4620c0ULL, 0x3fe05fb517af3300ULL, 0x4007336d1f4ee2e0ULL, 0xbffceb12ae50cbc0ULL, 0x4005f22b4fb8d620ULL, 0x3ff5680c58078280ULL, 0x40045ddc5ee69c40ULL, 0x400dad20306f88c0ULL, 0xc00db428a848cf80ULL, 0xbffd61f18e1d4f00ULL, 0xbffba92d0dfd3a00ULL, 0xc00aa5631d6e6b40ULL, 0x400780ad59555fc0ULL, 0xc00c31e8895af080ULL, 0xbff0d9a5b32db480ULL, 0x3fd584d0b9922a00ULL, 0xc010e0357d6a9a10ULL, 0x4011a9959c1208f0ULL, 0x400f6cd93651ce40ULL, 0x3ff642a1cdec9580ULL, 0x4009ca3e2a173140ULL, 0xc00cae7e63e89880ULL, 0xbff98c2dfccaa380ULL, 0xbfe0639f8cbe1a00ULL, 0x40034f9d1132c6c0ULL, 0x400407b3c4fb5000ULL, 0xbfff41169f40d980ULL, 0x4007700fc3805b40ULL, 0xc004aa3aa2af5500ULL, 0x40067af031ccab00ULL, 0xbfe9591daa8c9900ULL, 0xc00069d46757f080ULL, 0xc00a9f98e098a540ULL, 0xbfd55b2e06eb1400ULL, 0x4013ffe5157235e0ULL, 0xbff241498fc83800ULL, 0x3fe3a7c5a5ea5900ULL, 0xbff08158645ac280ULL, 0xc01280cc3db4b800ULL, 0x3fd3f4909d77ea00ULL, 0x400b590c9cb2bec0ULL, 0xbffb5b1a49701f00ULL, 0x3ffca8f4eddcd900ULL, 0xc00ae2e9b9dcb8c0ULL};
static const std::uint64_t k_eps_e_qtr3[3] = {0xbe4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_e_qtr3[3] = {0x3f887200048c60f0ULL, 0xbf962500041ec9b3ULL, 0x3f08000004772ae4ULL};
static const std::uint64_t k_hess_e_qtr3[9] = {0x40877beb505e979fULL, 0x403182400341fa73ULL, 0x3f10000002fa1c99ULL, 0x403182400341fa73ULL, 0x4086b8fcb83a529fULL, 0xbf0200000359602cULL, 0x3f10000002fa1c99ULL, 0xbf0200000359602cULL, 0x408ae438c500e51fULL};
static const std::uint64_t k_score_e_qtr3[3] = {0xbf569212dc4d2a03ULL, 0x3f64723e7e2daf23ULL, 0xbed628d0fba1e8f0ULL};
static const std::uint64_t k_info_e_qtr3[9] = {0x4055aedd60aefd5fULL, 0x40002a818b12fbb3ULL, 0x3edd8bc14f828becULL, 0x40002a818b12fbb3ULL, 0x4054fae1d0f7605aULL, 0xbed09e9cbcb96eb5ULL, 0x3edd8bc14f828becULL, 0xbed09e9cbcb96eb5ULL, 0x4058d44537c966d6ULL};
static const std::uint64_t k_asy_e_qtr3[9] = {0x3f87a04b832d87bcULL, 0xbf32347babf78bdbULL, 0xbe0c7ec27303c391ULL, 0xbf32347babf78bdbULL, 0x3f886afa716609bdULL, 0x3e010570f6f06fc3ULL, 0xbe0c7ec27303c391ULL, 0x3e010570f6f06fc3ULL, 0x3f849ef3235deeaeULL};
static const std::uint64_t k_diag_e_qtr3[3] = {0x3f87a04b832d87bcULL, 0x3f886afa716609bdULL, 0x3f849ef3235deeaeULL};
static const std::uint64_t k_nk_e_qtr3_0[3] = {0x3fc546acca74e385ULL, 0x3fb8407bce018cb8ULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_1[3] = {0x3fc546acea74e37fULL, 0x3fb8407b8e018cc4ULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_2[3] = {0x3fc546acea74e37fULL, 0x3fb8407bce018cb8ULL, 0xbfeffffff7fd1846ULL};
static const std::uint64_t k_nk_e_qtr3_3[3] = {0x3fc54aacea74843bULL, 0x3fb8407bce018cb8ULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_4[3] = {0x3fc542acea7542c3ULL, 0x3fb8407bce018cb8ULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_5[3] = {0x3fc546acea74e37fULL, 0x3fb8487bce00ce31ULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_6[3] = {0x3fc546acea74e37fULL, 0x3fb8387bce024b3fULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_7[3] = {0x3fc546acea74e37fULL, 0x3fb8407bce018cb8ULL, 0xbfeffefffffd3016ULL};
static const std::uint64_t k_nk_e_qtr3_8[3] = {0x3fc546acea74e37fULL, 0x3fb8407bce018cb8ULL, 0xbff0007ffffe803aULL};
static const std::uint64_t k_nk_e_qtr3_9[3] = {0x3fc548acea74b3ddULL, 0x3fb8447bce012d74ULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_10[3] = {0x3fc544acea751321ULL, 0x3fb83c7bce01ebfcULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_11[3] = {0x3fc544acea751321ULL, 0x3fb8447bce012d74ULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_12[3] = {0x3fc548acea74b3ddULL, 0x3fb83c7bce01ebfcULL, 0xbfeffffffffd1845ULL};
static const std::uint64_t k_nk_e_qtr3_13[3] = {0x3fc548acea74b3ddULL, 0x3fb8407bce018cb8ULL, 0xbfefff7ffffd242dULL};
static const std::uint64_t k_nk_e_qtr3_14[3] = {0x3fc544acea751321ULL, 0x3fb8407bce018cb8ULL, 0xbff0003ffffe862eULL};
static const std::uint64_t k_nk_e_qtr3_15[3] = {0x3fc544acea751321ULL, 0x3fb8407bce018cb8ULL, 0xbfefff7ffffd242dULL};
static const std::uint64_t k_nk_e_qtr3_16[3] = {0x3fc548acea74b3ddULL, 0x3fb8407bce018cb8ULL, 0xbff0003ffffe862eULL};
static const std::uint64_t k_nk_e_qtr3_17[3] = {0x3fc546acea74e37fULL, 0x3fb8447bce012d74ULL, 0xbfefff7ffffd242dULL};
static const std::uint64_t k_nk_e_qtr3_18[3] = {0x3fc546acea74e37fULL, 0x3fb83c7bce01ebfcULL, 0xbff0003ffffe862eULL};
static const std::uint64_t k_nk_e_qtr3_19[3] = {0x3fc546acea74e37fULL, 0x3fb83c7bce01ebfcULL, 0xbfefff7ffffd242dULL};
static const std::uint64_t k_nk_e_qtr3_20[3] = {0x3fc546acea74e37fULL, 0x3fb8447bce012d74ULL, 0xbff0003ffffe862eULL};
static const std::uint64_t k_nk_e_qtr3_21[3] = {0x3fc546acea74e37fULL, 0x3fb8407bce018cb8ULL, 0xbfeffffffffd1845ULL};
static const Node k_nodes_e_qtr3[22] = {
    {k_nk_e_qtr3_0, 0x4077d3cb2488b4b5ULL, 0},
    {k_nk_e_qtr3_1, 0x4077d3cb2488d713ULL, 0},
    {k_nk_e_qtr3_2, 0x4077d3cb2488c0faULL, 0},
    {k_nk_e_qtr3_3, 0x4077d3cb2befba2bULL, 0},
    {k_nk_e_qtr3_4, 0x4077d3cb28dfbd57ULL, 0},
    {k_nk_e_qtr3_5, 0x4077d3cb277308b0ULL, 0},
    {k_nk_e_qtr3_6, 0x4077d3cb2cfaf786ULL, 0},
    {k_nk_e_qtr3_7, 0x4077d3cb2b420509ULL, 0},
    {k_nk_e_qtr3_8, 0x4077d3cb2b419933ULL, 0},
    {k_nk_e_qtr3_9, 0x4077d3cb26df93b3ULL, 0},
    {k_nk_e_qtr3_10, 0x4077d3cb281b8fafULL, 0},
    {k_nk_e_qtr3_11, 0x4077d3cb25349044ULL, 0},
    {k_nk_e_qtr3_12, 0x4077d3cb29808a1eULL, 0},
    {k_nk_e_qtr3_13, 0x4077d3cb2872d749ULL, 0},
    {k_nk_e_qtr3_14, 0x4077d3cb26eaaf25ULL, 0},
    {k_nk_e_qtr3_15, 0x4077d3cb26eabcb9ULL, 0},
    {k_nk_e_qtr3_16, 0x4077d3cb2872c9a5ULL, 0},
    {k_nk_e_qtr3_17, 0x4077d3cb26407f63ULL, 0},
    {k_nk_e_qtr3_18, 0x4077d3cb2904a92eULL, 0},
    {k_nk_e_qtr3_19, 0x4077d3cb2904b6cdULL, 0},
    {k_nk_e_qtr3_20, 0x4077d3cb264071ccULL, 0},
    {k_nk_e_qtr3_21, 0x4077d3cb2488c0eeULL, 0},
};
static const std::uint64_t k_point_e_d2[2] = {0x3fdbc629bb8c2974ULL, 0xbfeffff2a4d32f10ULL};
static const std::uint64_t k_yd_e_d2[142] = {0x3ff4e29509500ba0ULL, 0xc0047b898629c550ULL, 0x400f3e93fb1720a0ULL, 0xc00da1e9fd3fc7d0ULL, 0x4001238d19d3cee0ULL, 0xc003ec23c0ae6080ULL, 0x4005245610c4a390ULL, 0xbffcf05f053eec00ULL, 0x400379cadcadb670ULL, 0xc000904cbae9b540ULL, 0x400eddae78ce1070ULL, 0xc011a8ee346f4c10ULL, 0xbfd9faaab270e500ULL, 0x401096035c677528ULL, 0xc0013bfe352723c0ULL, 0x3fbe4450c2c23c00ULL, 0xc00206df26403710ULL, 0x4008b7725ea45620ULL, 0xc00f7c5375eec9e0ULL, 0x401212b677463230ULL, 0xc00605aa98caed80ULL, 0x400958a27e56ecd0ULL, 0xc009c8387d8886b0ULL, 0x3fdab7bb0e269a80ULL, 0x4003681bc6be0c80ULL, 0xc01686dfbbe6c1f0ULL, 0x401e6332c14792a8ULL, 0xc01c0c3b218772d0ULL, 0x400e6f8de77b57c0ULL, 0xc0056c8011473400ULL, 0x4009f9a0c5d318d0ULL, 0xbfe0adcf0cdebf80ULL, 0xbfee4eda87d1c040ULL, 0x3fe6730db6ec3b40ULL, 0x3fe25d3bd6e07780ULL, 0xbff5990a06755560ULL, 0x3fe934110cea5bc0ULL, 0x40044b0f37e769f0ULL, 0xc0162d0d33373a28ULL, 0x400752159db15590ULL, 0x3ff8e07ae9238040ULL, 0xbff5ace2ad829aa0ULL, 0xbfd07d7dea044a80ULL, 0x3fea76a78931c340ULL, 0xbfd8526ca5106d00ULL, 0x3ff1bb0b57e07800ULL, 0xc002a67fe76d1820ULL, 0x3ff8eb05945812e0ULL, 0xc00062043fa38b00ULL, 0xbff08b5a683eb060ULL, 0x40107e082b9bf448ULL, 0xc0027b45531ce3a0ULL, 0x3ffaace754d2d280ULL, 0xbff3c072c201e5e0ULL, 0xbfe7d3f038e50e80ULL, 0x3feb2e0c882f2680ULL, 0xc00287c740d9f520ULL, 0x3ff22a1f238678e0ULL, 0x400aa66d919d5110ULL, 0xc00de2cb669cd3a0ULL, 0x400e03cf8f8a3da0ULL, 0xc0047f0761a4c1c0ULL, 0xbfcfbb8e3d87c000ULL, 0x40053205c4e15eb0ULL, 0xc0006e1c500e21d0ULL, 0x3febb2323f1c2680ULL, 0x3ff0392655bade60ULL, 0xc00c031ffc528f30ULL, 0x40116ebed5ee6ad8ULL, 0xc013e784b33070b8ULL, 0x3ffa922412703640ULL, 0x400a92bf804b3de0ULL, 0xc010ca109af855b8ULL, 0xbfd5f4cf08163100ULL, 0x4011bec6b4a1dd68ULL, 0xc00b055771f4a060ULL, 0xbfd7bc6db74f9400ULL, 0xbfe5a44ce0c1d040ULL, 0x4010e0ce6f351d20ULL, 0xbff3492dc8d802a0ULL, 0xc00a339d6a8127e0ULL, 0x40009eb282993100ULL, 0x3fd7d77fc1de5000ULL, 0xbfe06e8915c72ec0ULL, 0xbfcecd54e0437500ULL, 0xbfca332536f13b00ULL, 0x4007b275b1267e60ULL, 0xc00cda7b5578eb70ULL, 0xbffa895798ec0500ULL, 0x40176737f2e73028ULL, 0xbff85faf9ac235c0ULL, 0xc0149e3495dbfe40ULL, 0x4010f7813c215a88ULL, 0xbf9a273ce1ef4000ULL, 0xbfffb64c6c927240ULL, 0x40017c4daba38e60ULL, 0x3fd1c255d5310e00ULL, 0xc0167ffb0531fe40ULL, 0x4020093763984cf0ULL, 0xbffdc9aeb0409fc0ULL, 0xc0096ff6c2afd4a0ULL, 0x4004b1576c1e8e90ULL, 0xbff2e122391d52e0ULL, 0x3fe15fa75cc50c00ULL, 0xc004f13dbacaa6f0ULL, 0x400039ff60fb81b0ULL, 0x3ff4b379e406a640ULL, 0xc00b322543f83a70ULL, 0x3ffdfe577fe717a0ULL, 0xbff315126ad926a0ULL, 0x4014cc4cd9123fc8ULL, 0xc013910317556368ULL, 0x3ffc7368ee1a51c0ULL, 0xbff7ce244c84fb40ULL, 0x3ff7dd2f0c683be0ULL, 0x3fe6a308a4ce4ec0ULL, 0xc00b4d86368bb0b0ULL, 0x3fecffcddee8d700ULL, 0x400c14860864cfb0ULL, 0xbffaee897c3eb320ULL, 0x3fe91e5bf092de00ULL, 0xc0008da9f8ed9850ULL, 0x3fe2947e1d5ee3c0ULL, 0xbff36ab3757e7e00ULL, 0x3ff8b40686b60a00ULL, 0x3ff91c25596443a0ULL, 0xbffe3667b411a400ULL, 0xbf9a19551d92c000ULL, 0xc006750928a0dba0ULL, 0x3ffc37782e8c4c40ULL, 0x3ff9483c08f2aae0ULL, 0xbffdcf841a605c40ULL, 0x3fe8f9b7c4f7fa00ULL, 0x40123b180bcf6798ULL, 0xc0146b31c1560530ULL, 0x3ff5d49cb6fbcca0ULL, 0x3fe7d799c9985380ULL, 0xbff654b8f69a9be0ULL, 0xbfab56455ef0fc00ULL, 0x400d3b0284b07a00ULL, 0xc0132d39829d8900ULL, 0x40102ab742378e40ULL};
static const std::uint64_t k_eps_e_d2[2] = {0xbe4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_e_d2[2] = {0xbfec531005452679ULL, 0x3fafa48005e332ddULL};
static const std::uint64_t k_hess_e_d2[4] = {0x408ac41f2afaec22ULL, 0xbfad3980057006abULL, 0xbfad3980057006abULL, 0x40c2bfcb159d0faaULL};
static const std::uint64_t k_score_e_d2[2] = {0x3fc6cb29a77d2858ULL, 0xbf8976bb7a0013bdULL};
static const std::uint64_t k_info_e_d2[4] = {0x40658a1f4a25f1d3ULL, 0xbf87849a68f361f3ULL, 0xbf87849a68f361f3ULL, 0x409e2d18684dd1e7ULL};
static const std::uint64_t k_asy_e_d2[4] = {0x3f77c52e589886e3ULL, 0x3e6286867b0c5f33ULL, 0x3e6286867b0c5f33ULL, 0x3f40f78feb75b128ULL};
static const std::uint64_t k_diag_e_d2[2] = {0x3f77c52e589886e3ULL, 0x3f40f78feb75b128ULL};
static const std::uint64_t k_nk_e_d2_0[2] = {0x3fdbc629ab8c2977ULL, 0xbfeffff2a4d32f10ULL};
static const std::uint64_t k_nk_e_d2_1[2] = {0x3fdbc629bb8c2974ULL, 0xbfeffff29cd32f11ULL};
static const std::uint64_t k_nk_e_d2_2[2] = {0x3fdbc829bb8bf9d2ULL, 0xbfeffff2a4d32f10ULL};
static const std::uint64_t k_nk_e_d2_3[2] = {0x3fdbc429bb8c5916ULL, 0xbfeffff2a4d32f10ULL};
static const std::uint64_t k_nk_e_d2_4[2] = {0x3fdbc629bb8c2974ULL, 0xbfeffef2a4d346e1ULL};
static const std::uint64_t k_nk_e_d2_5[2] = {0x3fdbc629bb8c2974ULL, 0xbff0007952698ba0ULL};
static const std::uint64_t k_nk_e_d2_6[2] = {0x3fdbc729bb8c11a3ULL, 0xbfefff72a4d33af8ULL};
static const std::uint64_t k_nk_e_d2_7[2] = {0x3fdbc529bb8c4145ULL, 0xbff0003952699194ULL};
static const std::uint64_t k_nk_e_d2_8[2] = {0x3fdbc529bb8c4145ULL, 0xbfefff72a4d33af8ULL};
static const std::uint64_t k_nk_e_d2_9[2] = {0x3fdbc729bb8c11a3ULL, 0xbff0003952699194ULL};
static const std::uint64_t k_nk_e_d2_10[2] = {0x3fdbc629bb8c2974ULL, 0xbfeffff2a4d32f10ULL};
static const Node k_nodes_e_d2[11] = {
    {k_nk_e_d2_0, 0x4075bf11f7ae6235ULL, 0},
    {k_nk_e_d2_1, 0x4075bf11f7ab171cULL, 0},
    {k_nk_e_d2_2, 0x4075bf118d23303aULL, 0},
    {k_nk_e_d2_3, 0x4075bf126f948effULL, 0},
    {k_nk_e_d2_4, 0x4075bf124a7fa725ULL, 0},
    {k_nk_e_d2_5, 0x4075bf123ad46112ULL, 0},
    {k_nk_e_d2_6, 0x4075bf11d76437cdULL, 0},
    {k_nk_e_d2_7, 0x4075bf1240c98b54ULL, 0},
    {k_nk_e_d2_8, 0x4075bf12489d92c3ULL, 0},
    {k_nk_e_d2_9, 0x4075bf11cf906ad0ULL, 0},
    {k_nk_e_d2_10, 0x4075bf11f7aad7d3ULL, 0},
};
static const std::uint64_t k_point_e_arlim099[3] = {0x3f8b693466ad87f3ULL, 0xbfdde7a4055f47a4ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_yd_e_arlim099[107] = {0xbfd7fc8cf2fd8280ULL, 0x3ffc258c9025e140ULL, 0xc0068b5b2977fa70ULL, 0xbff0d03a18ca7880ULL, 0xbfeda167262e1580ULL, 0xc0106d5b4e0ae668ULL, 0x3ff929edfdbe4ca0ULL, 0xc008e638eb9e4aa0ULL, 0x40112e117489ddb8ULL, 0xbfe4ac0dc30ed4c0ULL, 0x4008a1c0bef2ce50ULL, 0x3fbf06855c83b400ULL, 0x3ff868cb03f5b400ULL, 0xbff447ed5f290280ULL, 0xbfcaa8644166f400ULL, 0x401223244bdaed80ULL, 0xc010f07878230538ULL, 0x401034d866692990ULL, 0xbff9c1c19549da60ULL, 0x3fccb1e9ed907100ULL, 0xbff71f0c02773c20ULL, 0xbff3c916b51a8e60ULL, 0xc00a79be2a856eb0ULL, 0x401626aa55edb6d8ULL, 0xbfec71381984d4c0ULL, 0xbfeca4a279266480ULL, 0x3fe6def58e369b80ULL, 0xc01323b9e8a2ec38ULL, 0x400657135ffd9910ULL, 0x3fd04d25f0567200ULL, 0x3fe8721a37907240ULL, 0x400045f442e2cae0ULL, 0xc00506a2413fa0e0ULL, 0x3feb342714d7df00ULL, 0x40049baca84432d0ULL, 0xc01cc84d2c1d0990ULL, 0x3fb70493d9c7e200ULL, 0x3ffb513cad026600ULL, 0xbff28c4faa2b1660ULL, 0x400a474f701ed180ULL, 0xbfc5ba62cdfb1500ULL, 0xc01455b5dc82a810ULL, 0x40169e2fd36f7710ULL, 0xc00964215dcf2ba0ULL, 0x400618b9264b55e0ULL, 0x3ff0c032b49b5140ULL, 0xc00626926e36ff20ULL, 0x3fe247ad9728e800ULL, 0x401287581f66ddf0ULL, 0xbff0237e19fcf340ULL, 0x400046cedb1e2530ULL, 0xbfec2fa71eaf10c0ULL, 0x3fffdc4bb2a19960ULL, 0x4004f12df024e990ULL, 0xbffacc05ae87c720ULL, 0x3fea932f8e69f2c0ULL, 0xbfdc2b78e53ff180ULL, 0xbff9802f4a9c6c40ULL, 0xbff555df6f348140ULL, 0x400456ed8656e5f0ULL, 0xc006674ce3c5fe10ULL, 0x400443e735bd3140ULL, 0xbfed3e8d17d22f00ULL, 0xc0099dd64773da90ULL, 0x3fc37cd84c273f00ULL, 0x4000757e65711ef0ULL, 0xc013c2702bf1a8a8ULL, 0x400cad7e8bc62020ULL, 0xc0015c4b38776810ULL, 0xbfe62c29ce513040ULL, 0x4010f9f2efdf1fa0ULL, 0xc003ef1078177200ULL, 0xc0083f225f028600ULL, 0xbfdd56fe09129200ULL, 0xc000d9593b767730ULL, 0x3ffb602cfc44b5e0ULL, 0x3ffbafd85a79d000ULL, 0xbfe1326e4efcf880ULL, 0x3fbaa75b4cb84600ULL, 0xc002478bb32d9c00ULL, 0x3fd2930c10ebce80ULL, 0x4011a3cc5e9fa308ULL, 0xc00b0cb68f591ff0ULL, 0x401210a36c915490ULL, 0x400356e359987f20ULL, 0xc00e9ea4334b97d0ULL, 0x3ff187176e7258a0ULL, 0x400ceb7677d5c000ULL, 0xc0131d50f7079cb0ULL, 0xc00a2bac301ffb20ULL, 0x40114ef974a12740ULL, 0x3ff16b096420bb80ULL, 0x3ffb3e089336ff80ULL, 0xc0031b5b2cc37e70ULL, 0x400c769244a6d800ULL, 0xc012881b3a832de8ULL, 0xbff46de30c485820ULL, 0x40001a41db8c6f20ULL, 0x3fe246d2ae3b2f40ULL, 0xc00b17a77b692680ULL, 0xbff18297dc83e9c0ULL, 0x401ae513de5fef38ULL, 0xc007ddefbe090f70ULL, 0xbfd2a4d6ff9fee00ULL, 0xbffef5a3cbda4d20ULL, 0x3fe9fb9e6023d740ULL, 0xbff09153896183a0ULL};
static const std::uint64_t k_eps_e_arlim099[3] = {0xbe4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_e_arlim099[3] = {0xbfa136800333e253ULL, 0xbfa2b880037bb445ULL, 0x3f816000033b9b0dULL};
static const std::uint64_t k_hess_e_arlim099[9] = {0x4087b8b07e69e63aULL, 0xc087872c0060af9cULL, 0xbf718c000343cadcULL, 0xc087872c0060af9cULL, 0x408c4a3949438177ULL, 0x3f6f200005ca8ba1ULL, 0xbf718c000343cadcULL, 0x3f6f200005ca8ba1ULL, 0x40801abc9eff161fULL};
static const std::uint64_t k_score_e_arlim099[3] = {0x3f72e70ece3a1240ULL, 0x3f748ef2f035ce04ULL, 0xbf5314a1b20bd9d6ULL};
static const std::uint64_t k_info_e_arlim099[9] = {0x405a0cce6ab4fb45ULL, 0xc059d66d8e02957eULL, 0xbf4344f368fcc61fULL, 0xc059d66d8e02957eULL, 0x405f112575e27746ULL, 0x3f4117156eaad84bULL, 0xbf4344f368fcc61fULL, 0x3f4117156eaad84bULL, 0x4051af707ab69132ULL};
static const std::uint64_t k_asy_e_arlim099[9] = {0x3fac0f86fa2e86eeULL, 0x3fa7565b0167aca9ULL, 0x3e800b4218e2861dULL, 0x3fa7565b0167aca9ULL, 0x3fa78778c200997aULL, 0x3e65858818c19e12ULL, 0x3e800b4218e2861dULL, 0x3e65858818c19e12ULL, 0x3f8cf35945c4a10bULL};
static const std::uint64_t k_diag_e_arlim099[3] = {0x3fac0f86fa2e86eeULL, 0x3fa78778c200997aULL, 0x3f8cf35945c4a10bULL};
static const std::uint64_t k_nk_e_arlim099_0[3] = {0x3f8b693266ad8852ULL, 0xbfdde7a4055f47a4ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_1[3] = {0x3f8b693466ad87f3ULL, 0xbfdde7a3f55f47a7ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_2[3] = {0x3f8b693466ad87f3ULL, 0xbfdde7a4055f47a4ULL, 0xbfefffdd844051a0ULL};
static const std::uint64_t k_nk_e_arlim099_3[3] = {0x3f8ba93466a793baULL, 0xbfdde7a4055f47a4ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_4[3] = {0x3f8b293466b37c2cULL, 0xbfdde7a4055f47a4ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_5[3] = {0x3f8b693466ad87f3ULL, 0xbfdde5a4055f7746ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_6[3] = {0x3f8b693466ad87f3ULL, 0xbfdde9a4055f1802ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_7[3] = {0x3f8b693466ad87f3ULL, 0xbfdde7a4055f47a4ULL, 0xbfeffedd8c406970ULL};
static const std::uint64_t k_nk_e_arlim099_8[3] = {0x3f8b693466ad87f3ULL, 0xbfdde7a4055f47a4ULL, 0xbff0006ec6201ce7ULL};
static const std::uint64_t k_nk_e_arlim099_9[3] = {0x3f8b893466aa8dd6ULL, 0xbfdde6a4055f5f75ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_10[3] = {0x3f8b493466b08210ULL, 0xbfdde8a4055f2fd3ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_11[3] = {0x3f8b493466b08210ULL, 0xbfdde6a4055f5f75ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_12[3] = {0x3f8b893466aa8dd6ULL, 0xbfdde8a4055f2fd3ULL, 0xbfefffdd8c40519fULL};
static const std::uint64_t k_nk_e_arlim099_13[3] = {0x3f8b893466aa8dd6ULL, 0xbfdde7a4055f47a4ULL, 0xbfefff5d8c405d87ULL};
static const std::uint64_t k_nk_e_arlim099_14[3] = {0x3f8b493466b08210ULL, 0xbfdde7a4055f47a4ULL, 0xbff0002ec62022dbULL};
static const std::uint64_t k_nk_e_arlim099_15[3] = {0x3f8b493466b08210ULL, 0xbfdde7a4055f47a4ULL, 0xbfefff5d8c405d87ULL};
static const std::uint64_t k_nk_e_arlim099_16[3] = {0x3f8b893466aa8dd6ULL, 0xbfdde7a4055f47a4ULL, 0xbff0002ec62022dbULL};
static const std::uint64_t k_nk_e_arlim099_17[3] = {0x3f8b693466ad87f3ULL, 0xbfdde6a4055f5f75ULL, 0xbfefff5d8c405d87ULL};
static const std::uint64_t k_nk_e_arlim099_18[3] = {0x3f8b693466ad87f3ULL, 0xbfdde8a4055f2fd3ULL, 0xbff0002ec62022dbULL};
static const std::uint64_t k_nk_e_arlim099_19[3] = {0x3f8b693466ad87f3ULL, 0xbfdde8a4055f2fd3ULL, 0xbfefff5d8c405d87ULL};
static const std::uint64_t k_nk_e_arlim099_20[3] = {0x3f8b693466ad87f3ULL, 0xbfdde6a4055f5f75ULL, 0xbff0002ec62022dbULL};
static const std::uint64_t k_nk_e_arlim099_21[3] = {0x3f8b693466ad87f3ULL, 0xbfdde7a4055f47a4ULL, 0xbfefffdd8c40519fULL};
static const Node k_nodes_e_arlim099[22] = {
    {k_nk_e_arlim099_0, 0x4077ad072180c54dULL, 0},
    {k_nk_e_arlim099_1, 0x4077ad0721807d6fULL, 0},
    {k_nk_e_arlim099_2, 0x4077ad072180ab90ULL, 0},
    {k_nk_e_arlim099_3, 0x4077ad07232157e5ULL, 0},
    {k_nk_e_arlim099_4, 0x4077ad072bbc4618ULL, 0},
    {k_nk_e_arlim099_5, 0x4077ad0723e44f37ULL, 0},
    {k_nk_e_arlim099_6, 0x4077ad072d42132bULL, 0},
    {k_nk_e_arlim099_7, 0x4077ad07269cdb0bULL, 0},
    {k_nk_e_arlim099_8, 0x4077ad072471c903ULL, 0},
    {k_nk_e_arlim099_9, 0x4077ad071d51d66bULL, 0},
    {k_nk_e_arlim099_10, 0x4077ad07264e018fULL, 0},
    {k_nk_e_arlim099_11, 0x4077ad0727810daaULL, 0},
    {k_nk_e_arlim099_12, 0x4077ad0727e2604eULL, 0},
    {k_nk_e_arlim099_13, 0x4077ad072261d347ULL, 0},
    {k_nk_e_arlim099_14, 0x4077ad072599dde6ULL, 0},
    {k_nk_e_arlim099_15, 0x4077ad0726af50eeULL, 0},
    {k_nk_e_arlim099_16, 0x4077ad07214c64a1ULL, 0},
    {k_nk_e_arlim099_17, 0x4077ad07227a4deeULL, 0},
    {k_nk_e_arlim099_18, 0x4077ad0726139881ULL, 0},
    {k_nk_e_arlim099_19, 0x4077ad072729076fULL, 0},
    {k_nk_e_arlim099_20, 0x4077ad072164db1dULL, 0},
    {k_nk_e_arlim099_21, 0x4077ad072180a2e0ULL, 0},
};
static const std::uint64_t k_point_i_arlim_pos[2] = {0x3fefffeb074a771dULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_yd_i_arlim_pos[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_arlim_pos[2] = {0xbe4ffffffa0bc6d1ULL, 0x0000000000000000ULL};
static const std::uint64_t k_grad_i_arlim_pos[2] = {0x411a80dae9346816ULL, 0x0000000000000000ULL};
static const std::uint64_t k_hess_i_arlim_pos[4] = {0xc1c63d5ac356ae81ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_score_i_arlim_pos[2] = {0xc0e7f67b2bda8263ULL, 0x8000000000000000ULL};
static const std::uint64_t k_info_i_arlim_pos[4] = {0xc1941b9bb4ed35deULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_asy_i_arlim_pos[1] = {0};
static const std::uint64_t k_diag_i_arlim_pos[1] = {0};
static const std::uint64_t k_nk_i_arlim_pos_0[2] = {0x3fefffeaff4a771eULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_pos_1[2] = {0x3fefffeb074a771dULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_pos_2[2] = {0x3ff0007583a52fa6ULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_pos_3[2] = {0x3feffeeb074a8eeeULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_pos_4[2] = {0x3ff0003583a5359aULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_pos_5[2] = {0x3fefff6b074a8305ULL, 0x3fe0000000000000ULL};
static const Node k_nodes_i_arlim_pos[6] = {
    {k_nk_i_arlim_pos_0, 0x407d0858c88eb2f4ULL, 0},
    {k_nk_i_arlim_pos_1, 0x407d08734969973aULL, 0},
    {k_nk_i_arlim_pos_2, 0x0000000000000000ULL, 1},
    {k_nk_i_arlim_pos_3, 0x407c5688736ffc54ULL, 0},
    {k_nk_i_arlim_pos_4, 0x0000000000000000ULL, 1},
    {k_nk_i_arlim_pos_5, 0x407c811a42207edaULL, 0},
};
static const std::uint64_t k_point_i_arlim_neg[2] = {0xbfefffeb074a771dULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_yd_i_arlim_neg[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_arlim_neg[2] = {0x3e4ffffffa0bc6d1ULL, 0x0000000000000000ULL};
static const std::uint64_t k_grad_i_arlim_neg[2] = {0xc135235b202bd99aULL, 0x0000000000000000ULL};
static const std::uint64_t k_hess_i_arlim_neg[4] = {0xc1e1bd0edb57a8f7ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_score_i_arlim_neg[2] = {0x40e7f6931ff2eed5ULL, 0x8000000000000000ULL};
static const std::uint64_t k_info_i_arlim_neg[4] = {0xc1941bf68114ac22ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_asy_i_arlim_neg[1] = {0};
static const std::uint64_t k_diag_i_arlim_neg[1] = {0};
static const std::uint64_t k_nk_i_arlim_neg_0[2] = {0xbfefffeaff4a771eULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_neg_1[2] = {0xbfefffeb074a771dULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_neg_2[2] = {0xbfeffeeb074a8eeeULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_neg_3[2] = {0xbff0007583a52fa6ULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_neg_4[2] = {0xbfefff6b074a8305ULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_nk_i_arlim_neg_5[2] = {0xbff0003583a5359aULL, 0x3fe0000000000000ULL};
static const Node k_nodes_i_arlim_neg[6] = {
    {k_nk_i_arlim_neg_0, 0x409727ae7c4b6e92ULL, 0},
    {k_nk_i_arlim_neg_1, 0x409727c39fa68acfULL, 0},
    {k_nk_i_arlim_neg_2, 0x409699db28e634e4ULL, 0},
    {k_nk_i_arlim_neg_3, 0x0000000000000000ULL, 1},
    {k_nk_i_arlim_neg_4, 0x4096bbcff94bfd2bULL, 0},
    {k_nk_i_arlim_neg_5, 0x0000000000000000ULL, 1},
};
static const std::uint64_t k_point_i_ar_over_pos[2] = {0x3ff0cccccccccccdULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_yd_i_ar_over_pos[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_ar_over_pos[1] = {0};
static const std::uint64_t k_grad_i_ar_over_pos[1] = {0};
static const std::uint64_t k_hess_i_ar_over_pos[1] = {0};
static const std::uint64_t k_score_i_ar_over_pos[1] = {0};
static const std::uint64_t k_info_i_ar_over_pos[1] = {0};
static const std::uint64_t k_asy_i_ar_over_pos[1] = {0};
static const std::uint64_t k_diag_i_ar_over_pos[1] = {0};
static const std::uint64_t k_nk_i_ar_over_pos_0[2] = {0x3ff0cccccccccccdULL, 0x3fe0000000000000ULL};
static const Node k_nodes_i_ar_over_pos[1] = {
    {k_nk_i_ar_over_pos_0, 0x0000000000000000ULL, 1},
};
static const std::uint64_t k_point_i_ar_over_neg[2] = {0xbff0cccccccccccdULL, 0xbfe0000000000000ULL};
static const std::uint64_t k_yd_i_ar_over_neg[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_ar_over_neg[1] = {0};
static const std::uint64_t k_grad_i_ar_over_neg[1] = {0};
static const std::uint64_t k_hess_i_ar_over_neg[1] = {0};
static const std::uint64_t k_score_i_ar_over_neg[1] = {0};
static const std::uint64_t k_info_i_ar_over_neg[1] = {0};
static const std::uint64_t k_asy_i_ar_over_neg[1] = {0};
static const std::uint64_t k_diag_i_ar_over_neg[1] = {0};
static const std::uint64_t k_nk_i_ar_over_neg_0[2] = {0xbff0cccccccccccdULL, 0xbfe0000000000000ULL};
static const Node k_nodes_i_ar_over_neg[1] = {
    {k_nk_i_ar_over_neg_0, 0x0000000000000000ULL, 1},
};
static const std::uint64_t k_point_i_bphi_at[2] = {0x3fd999999999999aULL, 0x3fefffeb074a771dULL};
static const std::uint64_t k_yd_i_bphi_at[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_bphi_at[2] = {0x0000000000000000ULL, 0xbe4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_bphi_at[2] = {0x0000000000000000ULL, 0x416b3f18e4bc6db2ULL};
static const std::uint64_t k_hess_i_bphi_at[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc21406e7d77041f8ULL};
static const std::uint64_t k_score_i_bphi_at[2] = {0x8000000000000000ULL, 0xc121f45d1e8de144ULL};
static const std::uint64_t k_info_i_bphi_at[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc1ca64ee2c907cfdULL};
static const std::uint64_t k_asy_i_bphi_at[1] = {0};
static const std::uint64_t k_diag_i_bphi_at[1] = {0};
static const std::uint64_t k_nk_i_bphi_at_0[2] = {0x3fd999999999999aULL, 0x3fefffeb074a771dULL};
static const std::uint64_t k_nk_i_bphi_at_1[2] = {0x3fd999999999999aULL, 0x3fefffeaff4a771eULL};
static const std::uint64_t k_nk_i_bphi_at_2[2] = {0x3fd999999999999aULL, 0x3ff0007583a52fa6ULL};
static const std::uint64_t k_nk_i_bphi_at_3[2] = {0x3fd999999999999aULL, 0x3feffeeb074a8eeeULL};
static const std::uint64_t k_nk_i_bphi_at_4[2] = {0x3fd999999999999aULL, 0x3ff0003583a5359aULL};
static const std::uint64_t k_nk_i_bphi_at_5[2] = {0x3fd999999999999aULL, 0x3fefff6b074a8305ULL};
static const Node k_nodes_i_bphi_at[6] = {
    {k_nk_i_bphi_at_0, 0x4093eae25e523d04ULL, 0},
    {k_nk_i_bphi_at_1, 0x4093ea08658b3fafULL, 0},
    {k_nk_i_bphi_at_2, 0x0000000000000000ULL, 1},
    {k_nk_i_bphi_at_3, 0x408dd250d2c94f62ULL, 0},
    {k_nk_i_bphi_at_4, 0x0000000000000000ULL, 1},
    {k_nk_i_bphi_at_5, 0x408ff889ccec8987ULL, 0},
};
static const std::uint64_t k_point_i_bphi_over[2] = {0x3fd999999999999aULL, 0x3ff3333333333333ULL};
static const std::uint64_t k_yd_i_bphi_over[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_bphi_over[1] = {0};
static const std::uint64_t k_grad_i_bphi_over[1] = {0};
static const std::uint64_t k_hess_i_bphi_over[1] = {0};
static const std::uint64_t k_score_i_bphi_over[1] = {0};
static const std::uint64_t k_info_i_bphi_over[1] = {0};
static const std::uint64_t k_asy_i_bphi_over[1] = {0};
static const std::uint64_t k_diag_i_bphi_over[1] = {0};
static const std::uint64_t k_nk_i_bphi_over_0[2] = {0x3fd999999999999aULL, 0x3ff3333333333333ULL};
static const Node k_nodes_i_bphi_over[1] = {
    {k_nk_i_bphi_over_0, 0x0000000000000000ULL, 1},
};
static const std::uint64_t k_point_i_nobs0[2] = {0xbfe0000000000000ULL, 0x3fc999999999999aULL};
static const std::uint64_t k_yd_i_nobs0[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_nobs0[2] = {0x3e4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_nobs0[2] = {0xc090d11503210396ULL, 0xc086ff4a0047673dULL};
static const std::uint64_t k_hess_i_nobs0[4] = {0x4090f28cc1273db9ULL, 0x408c3cb99940fe7eULL, 0x408c3cb99940fe7eULL, 0x409f90aed3df82efULL};
static const std::uint64_t k_score_i_nobs0[2] = {0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_info_i_nobs0[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_asy_i_nobs0[1] = {0};
static const std::uint64_t k_diag_i_nobs0[1] = {0};
static const std::uint64_t k_nk_i_nobs0_0[2] = {0xbfdffffff0000003ULL, 0x3fc999999999999aULL};
static const std::uint64_t k_nk_i_nobs0_1[2] = {0xbfe0000000000000ULL, 0x3fc99999799999a0ULL};
static const std::uint64_t k_nk_i_nobs0_2[2] = {0xbfdffe0000002fa2ULL, 0x3fc999999999999aULL};
static const std::uint64_t k_nk_i_nobs0_3[2] = {0xbfe000ffffffe82fULL, 0x3fc999999999999aULL};
static const std::uint64_t k_nk_i_nobs0_4[2] = {0xbfe0000000000000ULL, 0x3fc99d9999993a56ULL};
static const std::uint64_t k_nk_i_nobs0_5[2] = {0xbfe0000000000000ULL, 0x3fc995999999f8deULL};
static const std::uint64_t k_nk_i_nobs0_6[2] = {0xbfdfff00000017d1ULL, 0x3fc99b99999969f8ULL};
static const std::uint64_t k_nk_i_nobs0_7[2] = {0xbfe0007ffffff418ULL, 0x3fc997999999c93cULL};
static const std::uint64_t k_nk_i_nobs0_8[2] = {0xbfe0007ffffff418ULL, 0x3fc99b99999969f8ULL};
static const std::uint64_t k_nk_i_nobs0_9[2] = {0xbfdfff00000017d1ULL, 0x3fc997999999c93cULL};
static const std::uint64_t k_nk_i_nobs0_10[2] = {0xbfe0000000000000ULL, 0x3fc999999999999aULL};
static const Node k_nodes_i_nobs0[11] = {
    {k_nk_i_nobs0_0, 0x408cbabae4277bd7ULL, 0},
    {k_nk_i_nobs0_1, 0x408cbabaf24fd8d6ULL, 0},
    {k_nk_i_nobs0_2, 0x408cb9addf7c673cULL, 0},
    {k_nk_i_nobs0_3, 0x408cbbc8021cebd1ULL, 0},
    {k_nk_i_nobs0_4, 0x408cba02fa244a71ULL, 0},
    {k_nk_i_nobs0_5, 0x408cbb72eec419a4ULL, 0},
    {k_nk_i_nobs0_6, 0x408cb9d86b8be541ULL, 0},
    {k_nk_i_nobs0_7, 0x408cbb9d772c260eULL, 0},
    {k_nk_i_nobs0_8, 0x408cbae57954a0c1ULL, 0},
    {k_nk_i_nobs0_9, 0x408cba9062543c2aULL, 0},
    {k_nk_i_nobs0_10, 0x408cbabaec900657ULL, 0},
};
static const std::uint64_t k_point_i_nobsneg[2] = {0xbfe0000000000000ULL, 0x3fc999999999999aULL};
static const std::uint64_t k_yd_i_nobsneg[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_nobsneg[2] = {0x3e4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_nobsneg[2] = {0xc090d11503210396ULL, 0xc086ff4a0047673dULL};
static const std::uint64_t k_hess_i_nobsneg[4] = {0x4090f28cc1273db9ULL, 0x408c3cb99940fe7eULL, 0x408c3cb99940fe7eULL, 0x409f90aed3df82efULL};
static const std::uint64_t k_score_i_nobsneg[2] = {0xc00769f89d791969ULL, 0xc0000270a11f4f1dULL};
static const std::uint64_t k_info_i_nobsneg[4] = {0xc00798917e4fc389ULL, 0xc003a8480af31027ULL, 0xc003a8480af31027ULL, 0xc015f95f59763c68ULL};
static const std::uint64_t k_asy_i_nobsneg[4] = {0xbfe14b0c177766c3ULL, 0x3fcef0983b82a471ULL, 0x3fcef0983b82a470ULL, 0xbfd291cc2695b9a5ULL};
static const std::uint64_t k_diag_i_nobsneg[2] = {0xbfe14b0c177766c3ULL, 0xbfd291cc2695b9a5ULL};
static const std::uint64_t k_nk_i_nobsneg_0[2] = {0xbfdffffff0000003ULL, 0x3fc999999999999aULL};
static const std::uint64_t k_nk_i_nobsneg_1[2] = {0xbfe0000000000000ULL, 0x3fc99999799999a0ULL};
static const std::uint64_t k_nk_i_nobsneg_2[2] = {0xbfdffe0000002fa2ULL, 0x3fc999999999999aULL};
static const std::uint64_t k_nk_i_nobsneg_3[2] = {0xbfe000ffffffe82fULL, 0x3fc999999999999aULL};
static const std::uint64_t k_nk_i_nobsneg_4[2] = {0xbfe0000000000000ULL, 0x3fc99d9999993a56ULL};
static const std::uint64_t k_nk_i_nobsneg_5[2] = {0xbfe0000000000000ULL, 0x3fc995999999f8deULL};
static const std::uint64_t k_nk_i_nobsneg_6[2] = {0xbfdfff00000017d1ULL, 0x3fc99b99999969f8ULL};
static const std::uint64_t k_nk_i_nobsneg_7[2] = {0xbfe0007ffffff418ULL, 0x3fc997999999c93cULL};
static const std::uint64_t k_nk_i_nobsneg_8[2] = {0xbfe0007ffffff418ULL, 0x3fc99b99999969f8ULL};
static const std::uint64_t k_nk_i_nobsneg_9[2] = {0xbfdfff00000017d1ULL, 0x3fc997999999c93cULL};
static const std::uint64_t k_nk_i_nobsneg_10[2] = {0xbfe0000000000000ULL, 0x3fc999999999999aULL};
static const Node k_nodes_i_nobsneg[11] = {
    {k_nk_i_nobsneg_0, 0x408cbabae4277bd7ULL, 0},
    {k_nk_i_nobsneg_1, 0x408cbabaf24fd8d6ULL, 0},
    {k_nk_i_nobsneg_2, 0x408cb9addf7c673cULL, 0},
    {k_nk_i_nobsneg_3, 0x408cbbc8021cebd1ULL, 0},
    {k_nk_i_nobsneg_4, 0x408cba02fa244a71ULL, 0},
    {k_nk_i_nobsneg_5, 0x408cbb72eec419a4ULL, 0},
    {k_nk_i_nobsneg_6, 0x408cb9d86b8be541ULL, 0},
    {k_nk_i_nobsneg_7, 0x408cbb9d772c260eULL, 0},
    {k_nk_i_nobsneg_8, 0x408cbae57954a0c1ULL, 0},
    {k_nk_i_nobsneg_9, 0x408cba9062543c2aULL, 0},
    {k_nk_i_nobsneg_10, 0x408cbabaec900657ULL, 0},
};
static const std::uint64_t k_point_i_thetab[2] = {0xbfe0000000000000ULL, 0x3ff0000000000000ULL};
static const std::uint64_t k_yd_i_thetab[119] = {0xbffd775cbe81ae00ULL, 0x40099e5dac3412c0ULL, 0xc001ee96e0c0c150ULL, 0x3fd9f9092e658c80ULL, 0xbfe368340dd9c500ULL, 0xbfcfb0d8e76e9f00ULL, 0x3ff3c01c093e1fa0ULL, 0xbfd9d64548aab500ULL, 0xbfedbb701d413040ULL, 0x3ff4600298d19060ULL, 0xbfe7fcea990519c0ULL, 0xc003e3f3677babd0ULL, 0xbfe0febae0eadb00ULL, 0x3ff4f35a7383d0a0ULL, 0xbffbd1092b0a0680ULL, 0xbfe50b682b2c7640ULL, 0xbffa104f484d6220ULL, 0x3fe577c194e97240ULL, 0x3ff78f0b1606bec0ULL, 0xc00ae61a0d5bae70ULL, 0x3ff82f9ca73cba40ULL, 0xbfecbce24dcb5100ULL, 0x3fd5452a093c1980ULL, 0x3fc011bb73270300ULL, 0xbfd3aa8d0745cd80ULL, 0xbff14395ac7b00c0ULL, 0xbff4a5e0b307bb80ULL, 0x3ff8bfbc57210fe0ULL, 0x3fe781f5822ccdc0ULL, 0x3ff45d6cb60a41c0ULL, 0xbfee6687d756d7c0ULL, 0x3fea9dfafe2be9c0ULL, 0xbff39e864ec77800ULL, 0x3ffd3341161f6680ULL, 0xbff05f28a6d83fa0ULL, 0xbff2da6842e3ef00ULL, 0xbfd6664084ae3c00ULL, 0x3ff3de0303212f00ULL, 0xbff310735003c080ULL, 0x3fc376d374ffd100ULL, 0xbfff49fd3e395b20ULL, 0xbfe4d790edf68c40ULL, 0x4009748887217c00ULL, 0x3fd4b2d82d51de80ULL, 0xbfd1235a9e717180ULL, 0xbff2e8567d57a880ULL, 0xbfe7f476e8198240ULL, 0x3fca62f8168f8c00ULL, 0xbff957c44ccaab00ULL, 0xbfc8efa1799f7100ULL, 0x4001e166731eefb0ULL, 0xbfd6320a5e544c80ULL, 0xbff61f37b7879fa0ULL, 0x3ff97357165bb4e0ULL, 0x3fdc8e61d1aa3780ULL, 0xbff56aa4b7f2ba60ULL, 0xbfe7f1eb59786480ULL, 0x400c0156b1fa93a0ULL, 0xc00457826182f730ULL, 0x3fbcb73e3397b400ULL, 0xbff7c3a9e83828e0ULL, 0x400473f1098ddb20ULL, 0xbfa1cb28da501c00ULL, 0xbfd8550d8bb20300ULL, 0x3ffebe9bf76f88c0ULL, 0xbfe685c9105cd0c0ULL, 0xbfd28a7239985f80ULL, 0x3fe4d0ad36a3b340ULL, 0x3ffa43d96760fb60ULL, 0xbfeaf6b63c78d680ULL, 0xbff74b6baa567e20ULL, 0x3ff9baf30f299d60ULL, 0xbff7618f3b360c00ULL, 0x3ff67c8a1fe1afc0ULL, 0x3fd74fc492b38d00ULL, 0xc0062ee6440162f0ULL, 0xbf7c2da7e2668000ULL, 0xbfd375b741058a80ULL, 0x3fcc96c60c4cbb00ULL, 0xbff273adcb0e97a0ULL, 0xbfe5ac38597c8600ULL, 0x3fe51f5561182e40ULL, 0x3f99a4bbe9ada800ULL, 0x3ff8ce8a8167e7e0ULL, 0x3ff2917f703b5640ULL, 0xbfe84d69dab224c0ULL, 0x3fbb83c7eb5ba800ULL, 0x3ffc1dfbb0ef5480ULL, 0xbff74dd339e97440ULL, 0x3fff9fcc91256620ULL, 0x4002f828b3661350ULL, 0x3fc339ddf9b03500ULL, 0xc00b245e184246a0ULL, 0x3ffe686c060916a0ULL, 0x3ff256683135ae60ULL, 0xbff9bd5afba58620ULL, 0x3fe3ced50501e180ULL, 0x3fb33bc7ad9ad600ULL, 0xbfedc951a7c5d380ULL, 0xc005007315221ce0ULL, 0x401014e817c71508ULL, 0x3fd12195e46cf400ULL, 0xbfaac415d5035800ULL, 0xc00244980a5e2a70ULL, 0x3ff46d5a3c49aac0ULL, 0x400a0189c911e7c0ULL, 0xc001131d14fd9c90ULL, 0x3ff903b0cca5d800ULL, 0xbffa4476426cc600ULL, 0x3fe028f916d17780ULL, 0x3fecfb5656f15840ULL, 0xc0034065ff187f20ULL, 0x3ffd672439af74c0ULL, 0xc0036c478ebd0270ULL, 0xbfb725d58913a800ULL, 0x3faaee1fad132800ULL, 0x3ff7093eec0c5100ULL, 0xc0098cb1f5315530ULL, 0x3ffd2c512ae3c8c0ULL};
static const std::uint64_t k_eps_i_thetab[2] = {0x0000000000000000ULL, 0xbe4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_thetab[2] = {0x0000000000000000ULL, 0x3ff5740003fddad7ULL};
static const std::uint64_t k_hess_i_thetab[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc1800546167a97cbULL};
static const std::uint64_t k_score_i_thetab[2] = {0x8000000000000000ULL, 0xbf759b1042ce5e90ULL};
static const std::uint64_t k_info_i_thetab[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc1002271fe9909bcULL};
static const std::uint64_t k_asy_i_thetab[1] = {0};
static const std::uint64_t k_diag_i_thetab[1] = {0};
static const std::uint64_t k_nk_i_thetab_0[2] = {0xbfe0000000000000ULL, 0x3ff0000000000000ULL};
static const std::uint64_t k_nk_i_thetab_1[2] = {0xbfe0000000000000ULL, 0x3feffffff8000001ULL};
static const std::uint64_t k_nk_i_thetab_2[2] = {0xbfe0000000000000ULL, 0x3ff0007ffffff418ULL};
static const std::uint64_t k_nk_i_thetab_3[2] = {0xbfe0000000000000ULL, 0x3fefff00000017d1ULL};
static const std::uint64_t k_nk_i_thetab_4[2] = {0xbfe0000000000000ULL, 0x3ff0003ffffffa0cULL};
static const std::uint64_t k_nk_i_thetab_5[2] = {0xbfe0000000000000ULL, 0x3fefff8000000be8ULL};
static const Node k_nodes_i_thetab[6] = {
    {k_nk_i_thetab_0, 0x40cd0b1d959312faULL, 0},
    {k_nk_i_thetab_1, 0x40cd0b1d9592e812ULL, 0},
    {k_nk_i_thetab_2, 0x40cd0afd8c074219ULL, 0},
    {k_nk_i_thetab_3, 0x40cd0afd8a0695ddULL, 0},
    {k_nk_i_thetab_4, 0x40cd0b159307c4dcULL, 0},
    {k_nk_i_thetab_5, 0x40cd0b1592c799f5ULL, 0},
};
static const std::uint64_t k_point_i_thetab_off[2] = {0xbfe0000000000000ULL, 0x3ff0000000000000ULL};
static const std::uint64_t k_yd_i_thetab_off[119] = {0xbffd775cbe81ae00ULL, 0x40099e5dac3412c0ULL, 0xc001ee96e0c0c150ULL, 0x3fd9f9092e658c80ULL, 0xbfe368340dd9c500ULL, 0xbfcfb0d8e76e9f00ULL, 0x3ff3c01c093e1fa0ULL, 0xbfd9d64548aab500ULL, 0xbfedbb701d413040ULL, 0x3ff4600298d19060ULL, 0xbfe7fcea990519c0ULL, 0xc003e3f3677babd0ULL, 0xbfe0febae0eadb00ULL, 0x3ff4f35a7383d0a0ULL, 0xbffbd1092b0a0680ULL, 0xbfe50b682b2c7640ULL, 0xbffa104f484d6220ULL, 0x3fe577c194e97240ULL, 0x3ff78f0b1606bec0ULL, 0xc00ae61a0d5bae70ULL, 0x3ff82f9ca73cba40ULL, 0xbfecbce24dcb5100ULL, 0x3fd5452a093c1980ULL, 0x3fc011bb73270300ULL, 0xbfd3aa8d0745cd80ULL, 0xbff14395ac7b00c0ULL, 0xbff4a5e0b307bb80ULL, 0x3ff8bfbc57210fe0ULL, 0x3fe781f5822ccdc0ULL, 0x3ff45d6cb60a41c0ULL, 0xbfee6687d756d7c0ULL, 0x3fea9dfafe2be9c0ULL, 0xbff39e864ec77800ULL, 0x3ffd3341161f6680ULL, 0xbff05f28a6d83fa0ULL, 0xbff2da6842e3ef00ULL, 0xbfd6664084ae3c00ULL, 0x3ff3de0303212f00ULL, 0xbff310735003c080ULL, 0x3fc376d374ffd100ULL, 0xbfff49fd3e395b20ULL, 0xbfe4d790edf68c40ULL, 0x4009748887217c00ULL, 0x3fd4b2d82d51de80ULL, 0xbfd1235a9e717180ULL, 0xbff2e8567d57a880ULL, 0xbfe7f476e8198240ULL, 0x3fca62f8168f8c00ULL, 0xbff957c44ccaab00ULL, 0xbfc8efa1799f7100ULL, 0x4001e166731eefb0ULL, 0xbfd6320a5e544c80ULL, 0xbff61f37b7879fa0ULL, 0x3ff97357165bb4e0ULL, 0x3fdc8e61d1aa3780ULL, 0xbff56aa4b7f2ba60ULL, 0xbfe7f1eb59786480ULL, 0x400c0156b1fa93a0ULL, 0xc00457826182f730ULL, 0x3fbcb73e3397b400ULL, 0xbff7c3a9e83828e0ULL, 0x400473f1098ddb20ULL, 0xbfa1cb28da501c00ULL, 0xbfd8550d8bb20300ULL, 0x3ffebe9bf76f88c0ULL, 0xbfe685c9105cd0c0ULL, 0xbfd28a7239985f80ULL, 0x3fe4d0ad36a3b340ULL, 0x3ffa43d96760fb60ULL, 0xbfeaf6b63c78d680ULL, 0xbff74b6baa567e20ULL, 0x3ff9baf30f299d60ULL, 0xbff7618f3b360c00ULL, 0x3ff67c8a1fe1afc0ULL, 0x3fd74fc492b38d00ULL, 0xc0062ee6440162f0ULL, 0xbf7c2da7e2668000ULL, 0xbfd375b741058a80ULL, 0x3fcc96c60c4cbb00ULL, 0xbff273adcb0e97a0ULL, 0xbfe5ac38597c8600ULL, 0x3fe51f5561182e40ULL, 0x3f99a4bbe9ada800ULL, 0x3ff8ce8a8167e7e0ULL, 0x3ff2917f703b5640ULL, 0xbfe84d69dab224c0ULL, 0x3fbb83c7eb5ba800ULL, 0x3ffc1dfbb0ef5480ULL, 0xbff74dd339e97440ULL, 0x3fff9fcc91256620ULL, 0x4002f828b3661350ULL, 0x3fc339ddf9b03500ULL, 0xc00b245e184246a0ULL, 0x3ffe686c060916a0ULL, 0x3ff256683135ae60ULL, 0xbff9bd5afba58620ULL, 0x3fe3ced50501e180ULL, 0x3fb33bc7ad9ad600ULL, 0xbfedc951a7c5d380ULL, 0xc005007315221ce0ULL, 0x401014e817c71508ULL, 0x3fd12195e46cf400ULL, 0xbfaac415d5035800ULL, 0xc00244980a5e2a70ULL, 0x3ff46d5a3c49aac0ULL, 0x400a0189c911e7c0ULL, 0xc001131d14fd9c90ULL, 0x3ff903b0cca5d800ULL, 0xbffa4476426cc600ULL, 0x3fe028f916d17780ULL, 0x3fecfb5656f15840ULL, 0xc0034065ff187f20ULL, 0x3ffd672439af74c0ULL, 0xc0036c478ebd0270ULL, 0xbfb725d58913a800ULL, 0x3faaee1fad132800ULL, 0x3ff7093eec0c5100ULL, 0xc0098cb1f5315530ULL, 0x3ffd2c512ae3c8c0ULL};
static const std::uint64_t k_eps_i_thetab_off[2] = {0x3e4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_thetab_off[2] = {0xc0d34b648597090eULL, 0x3ff5740003fddad7ULL};
static const std::uint64_t k_hess_i_thetab_off[4] = {0x40cbdcd8452f2787ULL, 0x3ffd6780057895bdULL, 0x3ffd6780057895bdULL, 0xc1800546167a97cbULL};
static const std::uint64_t k_score_i_thetab_off[2] = {0x40536e8689c9bbfdULL, 0xbf759b1042ce5e90ULL};
static const std::uint64_t k_info_i_thetab_off[4] = {0x404c0f9449e9b51cULL, 0x3f7d9d0aa8629809ULL, 0x3f7d9d0aa8629809ULL, 0xc1002271fe9909bcULL};
static const std::uint64_t k_asy_i_thetab_off[4] = {0x3f923efd9f956d0dULL, 0x3e10be94b04639baULL, 0x3e10be94b04639baULL, 0xbedfbbaf1579dbf4ULL};
static const std::uint64_t k_diag_i_thetab_off[2] = {0x3f923efd9f956d0dULL, 0xbedfbbaf1579dbf4ULL};
static const std::uint64_t k_nk_i_thetab_off_0[2] = {0xbfdffffff0000003ULL, 0x3ff0000000000000ULL};
static const std::uint64_t k_nk_i_thetab_off_1[2] = {0xbfe0000000000000ULL, 0x3feffffff8000001ULL};
static const std::uint64_t k_nk_i_thetab_off_2[2] = {0xbfdffe0000002fa2ULL, 0x3ff0000000000000ULL};
static const std::uint64_t k_nk_i_thetab_off_3[2] = {0xbfe000ffffffe82fULL, 0x3ff0000000000000ULL};
static const std::uint64_t k_nk_i_thetab_off_4[2] = {0xbfe0000000000000ULL, 0x3ff0007ffffff418ULL};
static const std::uint64_t k_nk_i_thetab_off_5[2] = {0xbfe0000000000000ULL, 0x3fefff00000017d1ULL};
static const std::uint64_t k_nk_i_thetab_off_6[2] = {0xbfdfff00000017d1ULL, 0x3ff0003ffffffa0cULL};
static const std::uint64_t k_nk_i_thetab_off_7[2] = {0xbfe0007ffffff418ULL, 0x3fefff8000000be8ULL};
static const std::uint64_t k_nk_i_thetab_off_8[2] = {0xbfe0007ffffff418ULL, 0x3ff0003ffffffa0cULL};
static const std::uint64_t k_nk_i_thetab_off_9[2] = {0xbfdfff00000017d1ULL, 0x3fefff8000000be8ULL};
static const std::uint64_t k_nk_i_thetab_off_10[2] = {0xbfe0000000000000ULL, 0x3ff0000000000000ULL};
static const Node k_nodes_i_thetab_off[11] = {
    {k_nk_i_thetab_off_0, 0x40cd0b1d8bed60b9ULL, 0},
    {k_nk_i_thetab_off_1, 0x40cd0b1d9592e812ULL, 0},
    {k_nk_i_thetab_off_2, 0x40cd09e8dd668b7bULL, 0},
    {k_nk_i_thetab_off_3, 0x40cd0c5254b6d089ULL, 0},
    {k_nk_i_thetab_off_4, 0x40cd0afd8c074219ULL, 0},
    {k_nk_i_thetab_off_5, 0x40cd0afd8a0695ddULL, 0},
    {k_nk_i_thetab_off_6, 0x40cd0a7b363d5916ULL, 0},
    {k_nk_i_thetab_off_7, 0x40cd0baff1501293ULL, 0},
    {k_nk_i_thetab_off_8, 0x40cd0baff18ff5f7ULL, 0},
    {k_nk_i_thetab_off_9, 0x40cd0a7b35fd3ae2ULL, 0},
    {k_nk_i_thetab_off_10, 0x40cd0b1d959312faULL, 0},
};
static const std::uint64_t k_point_i_thetaneb[2] = {0xbfe0000000000000ULL, 0xbff0000000000000ULL};
static const std::uint64_t k_yd_i_thetaneb[119] = {0xbffd775cbe81ae00ULL, 0x40099e5dac3412c0ULL, 0xc001ee96e0c0c150ULL, 0x3fd9f9092e658c80ULL, 0xbfe368340dd9c500ULL, 0xbfcfb0d8e76e9f00ULL, 0x3ff3c01c093e1fa0ULL, 0xbfd9d64548aab500ULL, 0xbfedbb701d413040ULL, 0x3ff4600298d19060ULL, 0xbfe7fcea990519c0ULL, 0xc003e3f3677babd0ULL, 0xbfe0febae0eadb00ULL, 0x3ff4f35a7383d0a0ULL, 0xbffbd1092b0a0680ULL, 0xbfe50b682b2c7640ULL, 0xbffa104f484d6220ULL, 0x3fe577c194e97240ULL, 0x3ff78f0b1606bec0ULL, 0xc00ae61a0d5bae70ULL, 0x3ff82f9ca73cba40ULL, 0xbfecbce24dcb5100ULL, 0x3fd5452a093c1980ULL, 0x3fc011bb73270300ULL, 0xbfd3aa8d0745cd80ULL, 0xbff14395ac7b00c0ULL, 0xbff4a5e0b307bb80ULL, 0x3ff8bfbc57210fe0ULL, 0x3fe781f5822ccdc0ULL, 0x3ff45d6cb60a41c0ULL, 0xbfee6687d756d7c0ULL, 0x3fea9dfafe2be9c0ULL, 0xbff39e864ec77800ULL, 0x3ffd3341161f6680ULL, 0xbff05f28a6d83fa0ULL, 0xbff2da6842e3ef00ULL, 0xbfd6664084ae3c00ULL, 0x3ff3de0303212f00ULL, 0xbff310735003c080ULL, 0x3fc376d374ffd100ULL, 0xbfff49fd3e395b20ULL, 0xbfe4d790edf68c40ULL, 0x4009748887217c00ULL, 0x3fd4b2d82d51de80ULL, 0xbfd1235a9e717180ULL, 0xbff2e8567d57a880ULL, 0xbfe7f476e8198240ULL, 0x3fca62f8168f8c00ULL, 0xbff957c44ccaab00ULL, 0xbfc8efa1799f7100ULL, 0x4001e166731eefb0ULL, 0xbfd6320a5e544c80ULL, 0xbff61f37b7879fa0ULL, 0x3ff97357165bb4e0ULL, 0x3fdc8e61d1aa3780ULL, 0xbff56aa4b7f2ba60ULL, 0xbfe7f1eb59786480ULL, 0x400c0156b1fa93a0ULL, 0xc00457826182f730ULL, 0x3fbcb73e3397b400ULL, 0xbff7c3a9e83828e0ULL, 0x400473f1098ddb20ULL, 0xbfa1cb28da501c00ULL, 0xbfd8550d8bb20300ULL, 0x3ffebe9bf76f88c0ULL, 0xbfe685c9105cd0c0ULL, 0xbfd28a7239985f80ULL, 0x3fe4d0ad36a3b340ULL, 0x3ffa43d96760fb60ULL, 0xbfeaf6b63c78d680ULL, 0xbff74b6baa567e20ULL, 0x3ff9baf30f299d60ULL, 0xbff7618f3b360c00ULL, 0x3ff67c8a1fe1afc0ULL, 0x3fd74fc492b38d00ULL, 0xc0062ee6440162f0ULL, 0xbf7c2da7e2668000ULL, 0xbfd375b741058a80ULL, 0x3fcc96c60c4cbb00ULL, 0xbff273adcb0e97a0ULL, 0xbfe5ac38597c8600ULL, 0x3fe51f5561182e40ULL, 0x3f99a4bbe9ada800ULL, 0x3ff8ce8a8167e7e0ULL, 0x3ff2917f703b5640ULL, 0xbfe84d69dab224c0ULL, 0x3fbb83c7eb5ba800ULL, 0x3ffc1dfbb0ef5480ULL, 0xbff74dd339e97440ULL, 0x3fff9fcc91256620ULL, 0x4002f828b3661350ULL, 0x3fc339ddf9b03500ULL, 0xc00b245e184246a0ULL, 0x3ffe686c060916a0ULL, 0x3ff256683135ae60ULL, 0xbff9bd5afba58620ULL, 0x3fe3ced50501e180ULL, 0x3fb33bc7ad9ad600ULL, 0xbfedc951a7c5d380ULL, 0xc005007315221ce0ULL, 0x401014e817c71508ULL, 0x3fd12195e46cf400ULL, 0xbfaac415d5035800ULL, 0xc00244980a5e2a70ULL, 0x3ff46d5a3c49aac0ULL, 0x400a0189c911e7c0ULL, 0xc001131d14fd9c90ULL, 0x3ff903b0cca5d800ULL, 0xbffa4476426cc600ULL, 0x3fe028f916d17780ULL, 0x3fecfb5656f15840ULL, 0xc0034065ff187f20ULL, 0x3ffd672439af74c0ULL, 0xc0036c478ebd0270ULL, 0xbfb725d58913a800ULL, 0x3faaee1fad132800ULL, 0x3ff7093eec0c5100ULL, 0xc0098cb1f5315530ULL, 0x3ffd2c512ae3c8c0ULL};
static const std::uint64_t k_eps_i_thetaneb[2] = {0x0000000000000000ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_thetaneb[2] = {0x0000000000000000ULL, 0xbf5040000306050aULL};
static const std::uint64_t k_hess_i_thetaneb[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc0fc08c2264f5338ULL};
static const std::uint64_t k_score_i_thetaneb[2] = {0x8000000000000000ULL, 0x3f2316f7ddf4c98cULL};
static const std::uint64_t k_info_i_thetaneb[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc0d077749cb781f8ULL};
static const std::uint64_t k_asy_i_thetaneb[1] = {0};
static const std::uint64_t k_diag_i_thetaneb[1] = {0};
static const std::uint64_t k_nk_i_thetaneb_0[2] = {0xbfe0000000000000ULL, 0xbff0000000000000ULL};
static const std::uint64_t k_nk_i_thetaneb_1[2] = {0xbfe0000000000000ULL, 0xbfeffffff8000001ULL};
static const std::uint64_t k_nk_i_thetaneb_2[2] = {0xbfe0000000000000ULL, 0xbfefff00000017d1ULL};
static const std::uint64_t k_nk_i_thetaneb_3[2] = {0xbfe0000000000000ULL, 0xbff0007ffffff418ULL};
static const std::uint64_t k_nk_i_thetaneb_4[2] = {0xbfe0000000000000ULL, 0xbfefff8000000be8ULL};
static const std::uint64_t k_nk_i_thetaneb_5[2] = {0xbfe0000000000000ULL, 0xbff0003ffffffa0cULL};
static const Node k_nodes_i_thetaneb[6] = {
    {k_nk_i_thetaneb_0, 0x4078e61c8a8ded87ULL, 0},
    {k_nk_i_thetaneb_1, 0x4078e61c8a8dec83ULL, 0},
    {k_nk_i_thetaneb_2, 0x4078e6190959a0f1ULL, 0},
    {k_nk_i_thetaneb_3, 0x4078e6190991b1d7ULL, 0},
    {k_nk_i_thetaneb_4, 0x4078e61baa44226dULL, 0},
    {k_nk_i_thetaneb_5, 0x4078e61baa4b2490ULL, 0},
};
static const std::uint64_t k_point_i_theta_over[2] = {0xbfe0000000000000ULL, 0x3ff8000000000000ULL};
static const std::uint64_t k_yd_i_theta_over[119] = {0xbffd775cbe81ae00ULL, 0x40099e5dac3412c0ULL, 0xc001ee96e0c0c150ULL, 0x3fd9f9092e658c80ULL, 0xbfe368340dd9c500ULL, 0xbfcfb0d8e76e9f00ULL, 0x3ff3c01c093e1fa0ULL, 0xbfd9d64548aab500ULL, 0xbfedbb701d413040ULL, 0x3ff4600298d19060ULL, 0xbfe7fcea990519c0ULL, 0xc003e3f3677babd0ULL, 0xbfe0febae0eadb00ULL, 0x3ff4f35a7383d0a0ULL, 0xbffbd1092b0a0680ULL, 0xbfe50b682b2c7640ULL, 0xbffa104f484d6220ULL, 0x3fe577c194e97240ULL, 0x3ff78f0b1606bec0ULL, 0xc00ae61a0d5bae70ULL, 0x3ff82f9ca73cba40ULL, 0xbfecbce24dcb5100ULL, 0x3fd5452a093c1980ULL, 0x3fc011bb73270300ULL, 0xbfd3aa8d0745cd80ULL, 0xbff14395ac7b00c0ULL, 0xbff4a5e0b307bb80ULL, 0x3ff8bfbc57210fe0ULL, 0x3fe781f5822ccdc0ULL, 0x3ff45d6cb60a41c0ULL, 0xbfee6687d756d7c0ULL, 0x3fea9dfafe2be9c0ULL, 0xbff39e864ec77800ULL, 0x3ffd3341161f6680ULL, 0xbff05f28a6d83fa0ULL, 0xbff2da6842e3ef00ULL, 0xbfd6664084ae3c00ULL, 0x3ff3de0303212f00ULL, 0xbff310735003c080ULL, 0x3fc376d374ffd100ULL, 0xbfff49fd3e395b20ULL, 0xbfe4d790edf68c40ULL, 0x4009748887217c00ULL, 0x3fd4b2d82d51de80ULL, 0xbfd1235a9e717180ULL, 0xbff2e8567d57a880ULL, 0xbfe7f476e8198240ULL, 0x3fca62f8168f8c00ULL, 0xbff957c44ccaab00ULL, 0xbfc8efa1799f7100ULL, 0x4001e166731eefb0ULL, 0xbfd6320a5e544c80ULL, 0xbff61f37b7879fa0ULL, 0x3ff97357165bb4e0ULL, 0x3fdc8e61d1aa3780ULL, 0xbff56aa4b7f2ba60ULL, 0xbfe7f1eb59786480ULL, 0x400c0156b1fa93a0ULL, 0xc00457826182f730ULL, 0x3fbcb73e3397b400ULL, 0xbff7c3a9e83828e0ULL, 0x400473f1098ddb20ULL, 0xbfa1cb28da501c00ULL, 0xbfd8550d8bb20300ULL, 0x3ffebe9bf76f88c0ULL, 0xbfe685c9105cd0c0ULL, 0xbfd28a7239985f80ULL, 0x3fe4d0ad36a3b340ULL, 0x3ffa43d96760fb60ULL, 0xbfeaf6b63c78d680ULL, 0xbff74b6baa567e20ULL, 0x3ff9baf30f299d60ULL, 0xbff7618f3b360c00ULL, 0x3ff67c8a1fe1afc0ULL, 0x3fd74fc492b38d00ULL, 0xc0062ee6440162f0ULL, 0xbf7c2da7e2668000ULL, 0xbfd375b741058a80ULL, 0x3fcc96c60c4cbb00ULL, 0xbff273adcb0e97a0ULL, 0xbfe5ac38597c8600ULL, 0x3fe51f5561182e40ULL, 0x3f99a4bbe9ada800ULL, 0x3ff8ce8a8167e7e0ULL, 0x3ff2917f703b5640ULL, 0xbfe84d69dab224c0ULL, 0x3fbb83c7eb5ba800ULL, 0x3ffc1dfbb0ef5480ULL, 0xbff74dd339e97440ULL, 0x3fff9fcc91256620ULL, 0x4002f828b3661350ULL, 0x3fc339ddf9b03500ULL, 0xc00b245e184246a0ULL, 0x3ffe686c060916a0ULL, 0x3ff256683135ae60ULL, 0xbff9bd5afba58620ULL, 0x3fe3ced50501e180ULL, 0x3fb33bc7ad9ad600ULL, 0xbfedc951a7c5d380ULL, 0xc005007315221ce0ULL, 0x401014e817c71508ULL, 0x3fd12195e46cf400ULL, 0xbfaac415d5035800ULL, 0xc00244980a5e2a70ULL, 0x3ff46d5a3c49aac0ULL, 0x400a0189c911e7c0ULL, 0xc001131d14fd9c90ULL, 0x3ff903b0cca5d800ULL, 0xbffa4476426cc600ULL, 0x3fe028f916d17780ULL, 0x3fecfb5656f15840ULL, 0xc0034065ff187f20ULL, 0x3ffd672439af74c0ULL, 0xc0036c478ebd0270ULL, 0xbfb725d58913a800ULL, 0x3faaee1fad132800ULL, 0x3ff7093eec0c5100ULL, 0xc0098cb1f5315530ULL, 0x3ffd2c512ae3c8c0ULL};
static const std::uint64_t k_eps_i_theta_over[2] = {0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_grad_i_theta_over[2] = {0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_hess_i_theta_over[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_score_i_theta_over[2] = {0x8000000000000000ULL, 0x8000000000000000ULL};
static const std::uint64_t k_info_i_theta_over[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_asy_i_theta_over[1] = {0};
static const std::uint64_t k_diag_i_theta_over[1] = {0};
static const std::uint64_t k_nk_i_theta_over_0[2] = {0xbfe0000000000000ULL, 0x3ff8000000000000ULL};
static const Node k_nodes_i_theta_over[1] = {
    {k_nk_i_theta_over_0, 0x40946ffbc77a23c4ULL, 0},
};
static const std::uint64_t k_point_i_nan_phi[2] = {0x7ff8000000000000ULL, 0x3fd3333333333333ULL};
static const std::uint64_t k_yd_i_nan_phi[119] = {0xbffd775cbe81ae00ULL, 0x40099e5dac3412c0ULL, 0xc001ee96e0c0c150ULL, 0x3fd9f9092e658c80ULL, 0xbfe368340dd9c500ULL, 0xbfcfb0d8e76e9f00ULL, 0x3ff3c01c093e1fa0ULL, 0xbfd9d64548aab500ULL, 0xbfedbb701d413040ULL, 0x3ff4600298d19060ULL, 0xbfe7fcea990519c0ULL, 0xc003e3f3677babd0ULL, 0xbfe0febae0eadb00ULL, 0x3ff4f35a7383d0a0ULL, 0xbffbd1092b0a0680ULL, 0xbfe50b682b2c7640ULL, 0xbffa104f484d6220ULL, 0x3fe577c194e97240ULL, 0x3ff78f0b1606bec0ULL, 0xc00ae61a0d5bae70ULL, 0x3ff82f9ca73cba40ULL, 0xbfecbce24dcb5100ULL, 0x3fd5452a093c1980ULL, 0x3fc011bb73270300ULL, 0xbfd3aa8d0745cd80ULL, 0xbff14395ac7b00c0ULL, 0xbff4a5e0b307bb80ULL, 0x3ff8bfbc57210fe0ULL, 0x3fe781f5822ccdc0ULL, 0x3ff45d6cb60a41c0ULL, 0xbfee6687d756d7c0ULL, 0x3fea9dfafe2be9c0ULL, 0xbff39e864ec77800ULL, 0x3ffd3341161f6680ULL, 0xbff05f28a6d83fa0ULL, 0xbff2da6842e3ef00ULL, 0xbfd6664084ae3c00ULL, 0x3ff3de0303212f00ULL, 0xbff310735003c080ULL, 0x3fc376d374ffd100ULL, 0xbfff49fd3e395b20ULL, 0xbfe4d790edf68c40ULL, 0x4009748887217c00ULL, 0x3fd4b2d82d51de80ULL, 0xbfd1235a9e717180ULL, 0xbff2e8567d57a880ULL, 0xbfe7f476e8198240ULL, 0x3fca62f8168f8c00ULL, 0xbff957c44ccaab00ULL, 0xbfc8efa1799f7100ULL, 0x4001e166731eefb0ULL, 0xbfd6320a5e544c80ULL, 0xbff61f37b7879fa0ULL, 0x3ff97357165bb4e0ULL, 0x3fdc8e61d1aa3780ULL, 0xbff56aa4b7f2ba60ULL, 0xbfe7f1eb59786480ULL, 0x400c0156b1fa93a0ULL, 0xc00457826182f730ULL, 0x3fbcb73e3397b400ULL, 0xbff7c3a9e83828e0ULL, 0x400473f1098ddb20ULL, 0xbfa1cb28da501c00ULL, 0xbfd8550d8bb20300ULL, 0x3ffebe9bf76f88c0ULL, 0xbfe685c9105cd0c0ULL, 0xbfd28a7239985f80ULL, 0x3fe4d0ad36a3b340ULL, 0x3ffa43d96760fb60ULL, 0xbfeaf6b63c78d680ULL, 0xbff74b6baa567e20ULL, 0x3ff9baf30f299d60ULL, 0xbff7618f3b360c00ULL, 0x3ff67c8a1fe1afc0ULL, 0x3fd74fc492b38d00ULL, 0xc0062ee6440162f0ULL, 0xbf7c2da7e2668000ULL, 0xbfd375b741058a80ULL, 0x3fcc96c60c4cbb00ULL, 0xbff273adcb0e97a0ULL, 0xbfe5ac38597c8600ULL, 0x3fe51f5561182e40ULL, 0x3f99a4bbe9ada800ULL, 0x3ff8ce8a8167e7e0ULL, 0x3ff2917f703b5640ULL, 0xbfe84d69dab224c0ULL, 0x3fbb83c7eb5ba800ULL, 0x3ffc1dfbb0ef5480ULL, 0xbff74dd339e97440ULL, 0x3fff9fcc91256620ULL, 0x4002f828b3661350ULL, 0x3fc339ddf9b03500ULL, 0xc00b245e184246a0ULL, 0x3ffe686c060916a0ULL, 0x3ff256683135ae60ULL, 0xbff9bd5afba58620ULL, 0x3fe3ced50501e180ULL, 0x3fb33bc7ad9ad600ULL, 0xbfedc951a7c5d380ULL, 0xc005007315221ce0ULL, 0x401014e817c71508ULL, 0x3fd12195e46cf400ULL, 0xbfaac415d5035800ULL, 0xc00244980a5e2a70ULL, 0x3ff46d5a3c49aac0ULL, 0x400a0189c911e7c0ULL, 0xc001131d14fd9c90ULL, 0x3ff903b0cca5d800ULL, 0xbffa4476426cc600ULL, 0x3fe028f916d17780ULL, 0x3fecfb5656f15840ULL, 0xc0034065ff187f20ULL, 0x3ffd672439af74c0ULL, 0xc0036c478ebd0270ULL, 0xbfb725d58913a800ULL, 0x3faaee1fad132800ULL, 0x3ff7093eec0c5100ULL, 0xc0098cb1f5315530ULL, 0x3ffd2c512ae3c8c0ULL};
static const std::uint64_t k_eps_i_nan_phi[1] = {0};
static const std::uint64_t k_grad_i_nan_phi[1] = {0};
static const std::uint64_t k_hess_i_nan_phi[1] = {0};
static const std::uint64_t k_score_i_nan_phi[1] = {0};
static const std::uint64_t k_info_i_nan_phi[1] = {0};
static const std::uint64_t k_asy_i_nan_phi[1] = {0};
static const std::uint64_t k_diag_i_nan_phi[1] = {0};
static const std::uint64_t k_nk_i_nan_phi_0[2] = {0x7ff8000000000000ULL, 0x3fd3333333333333ULL};
static const Node k_nodes_i_nan_phi[1] = {
    {k_nk_i_nan_phi_0, 0x0000000000000000ULL, 1},
};
static const std::uint64_t k_point_i_nan_both[2] = {0x7ff8000000000000ULL, 0x7ff8000000000000ULL};
static const std::uint64_t k_yd_i_nan_both[119] = {0xbffd775cbe81ae00ULL, 0x40099e5dac3412c0ULL, 0xc001ee96e0c0c150ULL, 0x3fd9f9092e658c80ULL, 0xbfe368340dd9c500ULL, 0xbfcfb0d8e76e9f00ULL, 0x3ff3c01c093e1fa0ULL, 0xbfd9d64548aab500ULL, 0xbfedbb701d413040ULL, 0x3ff4600298d19060ULL, 0xbfe7fcea990519c0ULL, 0xc003e3f3677babd0ULL, 0xbfe0febae0eadb00ULL, 0x3ff4f35a7383d0a0ULL, 0xbffbd1092b0a0680ULL, 0xbfe50b682b2c7640ULL, 0xbffa104f484d6220ULL, 0x3fe577c194e97240ULL, 0x3ff78f0b1606bec0ULL, 0xc00ae61a0d5bae70ULL, 0x3ff82f9ca73cba40ULL, 0xbfecbce24dcb5100ULL, 0x3fd5452a093c1980ULL, 0x3fc011bb73270300ULL, 0xbfd3aa8d0745cd80ULL, 0xbff14395ac7b00c0ULL, 0xbff4a5e0b307bb80ULL, 0x3ff8bfbc57210fe0ULL, 0x3fe781f5822ccdc0ULL, 0x3ff45d6cb60a41c0ULL, 0xbfee6687d756d7c0ULL, 0x3fea9dfafe2be9c0ULL, 0xbff39e864ec77800ULL, 0x3ffd3341161f6680ULL, 0xbff05f28a6d83fa0ULL, 0xbff2da6842e3ef00ULL, 0xbfd6664084ae3c00ULL, 0x3ff3de0303212f00ULL, 0xbff310735003c080ULL, 0x3fc376d374ffd100ULL, 0xbfff49fd3e395b20ULL, 0xbfe4d790edf68c40ULL, 0x4009748887217c00ULL, 0x3fd4b2d82d51de80ULL, 0xbfd1235a9e717180ULL, 0xbff2e8567d57a880ULL, 0xbfe7f476e8198240ULL, 0x3fca62f8168f8c00ULL, 0xbff957c44ccaab00ULL, 0xbfc8efa1799f7100ULL, 0x4001e166731eefb0ULL, 0xbfd6320a5e544c80ULL, 0xbff61f37b7879fa0ULL, 0x3ff97357165bb4e0ULL, 0x3fdc8e61d1aa3780ULL, 0xbff56aa4b7f2ba60ULL, 0xbfe7f1eb59786480ULL, 0x400c0156b1fa93a0ULL, 0xc00457826182f730ULL, 0x3fbcb73e3397b400ULL, 0xbff7c3a9e83828e0ULL, 0x400473f1098ddb20ULL, 0xbfa1cb28da501c00ULL, 0xbfd8550d8bb20300ULL, 0x3ffebe9bf76f88c0ULL, 0xbfe685c9105cd0c0ULL, 0xbfd28a7239985f80ULL, 0x3fe4d0ad36a3b340ULL, 0x3ffa43d96760fb60ULL, 0xbfeaf6b63c78d680ULL, 0xbff74b6baa567e20ULL, 0x3ff9baf30f299d60ULL, 0xbff7618f3b360c00ULL, 0x3ff67c8a1fe1afc0ULL, 0x3fd74fc492b38d00ULL, 0xc0062ee6440162f0ULL, 0xbf7c2da7e2668000ULL, 0xbfd375b741058a80ULL, 0x3fcc96c60c4cbb00ULL, 0xbff273adcb0e97a0ULL, 0xbfe5ac38597c8600ULL, 0x3fe51f5561182e40ULL, 0x3f99a4bbe9ada800ULL, 0x3ff8ce8a8167e7e0ULL, 0x3ff2917f703b5640ULL, 0xbfe84d69dab224c0ULL, 0x3fbb83c7eb5ba800ULL, 0x3ffc1dfbb0ef5480ULL, 0xbff74dd339e97440ULL, 0x3fff9fcc91256620ULL, 0x4002f828b3661350ULL, 0x3fc339ddf9b03500ULL, 0xc00b245e184246a0ULL, 0x3ffe686c060916a0ULL, 0x3ff256683135ae60ULL, 0xbff9bd5afba58620ULL, 0x3fe3ced50501e180ULL, 0x3fb33bc7ad9ad600ULL, 0xbfedc951a7c5d380ULL, 0xc005007315221ce0ULL, 0x401014e817c71508ULL, 0x3fd12195e46cf400ULL, 0xbfaac415d5035800ULL, 0xc00244980a5e2a70ULL, 0x3ff46d5a3c49aac0ULL, 0x400a0189c911e7c0ULL, 0xc001131d14fd9c90ULL, 0x3ff903b0cca5d800ULL, 0xbffa4476426cc600ULL, 0x3fe028f916d17780ULL, 0x3fecfb5656f15840ULL, 0xc0034065ff187f20ULL, 0x3ffd672439af74c0ULL, 0xc0036c478ebd0270ULL, 0xbfb725d58913a800ULL, 0x3faaee1fad132800ULL, 0x3ff7093eec0c5100ULL, 0xc0098cb1f5315530ULL, 0x3ffd2c512ae3c8c0ULL};
static const std::uint64_t k_eps_i_nan_both[1] = {0};
static const std::uint64_t k_grad_i_nan_both[1] = {0};
static const std::uint64_t k_hess_i_nan_both[1] = {0};
static const std::uint64_t k_score_i_nan_both[1] = {0};
static const std::uint64_t k_info_i_nan_both[1] = {0};
static const std::uint64_t k_asy_i_nan_both[1] = {0};
static const std::uint64_t k_diag_i_nan_both[1] = {0};
static const std::uint64_t k_nk_i_nan_both_0[2] = {0x7ff8000000000000ULL, 0x7ff8000000000000ULL};
static const Node k_nodes_i_nan_both[1] = {
    {k_nk_i_nan_both_0, 0x0000000000000000ULL, 1},
};
static const std::uint64_t k_point_i_arlim099_inj[2] = {0x3fefd70a3d70a3d7ULL, 0x3fe0000000000000ULL};
static const std::uint64_t k_yd_i_arlim099_inj[107] = {0x3fe382e199d4c400ULL, 0xbfebd3f941f0a8c0ULL, 0x3fe4e50b09141580ULL, 0xc00a154673895f20ULL, 0x4003bd6b45cb1220ULL, 0x3fbce7a659413c00ULL, 0xbfec533827f55300ULL, 0x3fe892832a4eedc0ULL, 0xbfe09c9cc13ef340ULL, 0xc0000645fddb24c0ULL, 0xbfc716b2a6160900ULL, 0x3fe7e8a453d50800ULL, 0xbfd46b7f818b6d00ULL, 0xbfde1e91104ca080ULL, 0xbff843c6e0937a80ULL, 0xbff22496eff23060ULL, 0xc01108b08db931c0ULL, 0x400189408bdd05c0ULL, 0xbff3277c72cce8e0ULL, 0x3fcaef1ec0ef4600ULL, 0xbff26a32b9747360ULL, 0xbfe1a43b38031780ULL, 0x40057ea7c7d63ad0ULL, 0xc01007c88125ace8ULL, 0x3fe3390df401e000ULL, 0xc00289349f1fb490ULL, 0x401072920669b4f8ULL, 0xbfeea469b3cde800ULL, 0x3ffd68c824d6fa00ULL, 0xbfd8be58e7bc1c80ULL, 0xbfe0e99bfeda9600ULL, 0x3fe77459d96d50c0ULL, 0x3ff711eb4f31f660ULL, 0x3ff4f5ad0f743f20ULL, 0xbffdbd3fb6c09040ULL, 0x4002adafcce29bb0ULL, 0xbfdf9d434242ac00ULL, 0x4009c7552d4c1e00ULL, 0xbff96cfca5dc8640ULL, 0x40137d0d3a7547a8ULL, 0xc011092f392c4c70ULL, 0x3fe29e78b5312200ULL, 0xbfe9b8f1469c3200ULL, 0xbfdbf45102831380ULL, 0xbfafba0ed77f1000ULL, 0xbfbc3d51b7c08000ULL, 0xbff377088a0c4d20ULL, 0x3fd9dfba345cfc00ULL, 0x3ff4ebe67cc6f080ULL, 0xbffeaf04b0735920ULL, 0xbff601618435a340ULL, 0xbff946a8384c9860ULL, 0x4014d91392b75400ULL, 0xc001fed5793a8b70ULL, 0x3ff43d690560b060ULL, 0x3fdab21b2a8f2780ULL, 0x3fe41ff132b0e0c0ULL, 0x3ff9009e21d26da0ULL, 0x3ff29357b40443c0ULL, 0xc000b60b683056d0ULL, 0x3ffa23f89bb6c220ULL, 0x3ff0e271a74080e0ULL, 0xbfd172a90c2fa580ULL, 0x3febcb231e3fda80ULL, 0xc00cdffc98097e70ULL, 0x3ff9ac8399374a20ULL, 0xbff710232547d7e0ULL, 0x3ff731175d3286a0ULL, 0xbffb71a9acf2d0c0ULL, 0xc004d4d5961651c0ULL, 0x3fc058a188ea6100ULL, 0x3ff4e02d1f301620ULL, 0xc00bd9d3c3bda6b0ULL, 0x4016026df0701db8ULL, 0xc005fa45d491b6f0ULL, 0x3fee92478e41c000ULL, 0xbff7702d84fabfc0ULL, 0x3ff85f49dfe47120ULL, 0x3ff624918d191d20ULL, 0xbfe5d400132b42c0ULL, 0xc0063c83641d8f50ULL, 0x40169b74d6f34020ULL, 0xc00b05f717fe89a0ULL, 0x3fee35e25bd83e40ULL, 0x3ff4688c593eb7c0ULL, 0xc01a6d461d851d18ULL, 0x401b012cfcbc0c00ULL, 0xc004485ea9991c90ULL, 0x4008f18bf0eb37f0ULL, 0xc0151c2490f270d0ULL, 0x3ff1d6d3cc472f00ULL, 0x3fe595b34c420fc0ULL, 0x4008b4106d329430ULL, 0xbfee831107214300ULL, 0x400616a122b936a0ULL, 0xc0089e7b9c1e44e0ULL, 0x3fe33231f3150d80ULL, 0x4004b5ce66477f80ULL, 0xc01343d9865c9c98ULL, 0x3ff0c7248a8e00e0ULL, 0xbff21740ca9c7c60ULL, 0x40104f5a35619810ULL, 0x3fedb15d25ae1900ULL, 0xc0058aaa7c588110ULL, 0xbfdc70d2fc273400ULL, 0xc00f01de80594d90ULL, 0x3ff7270b129eeea0ULL};
static const std::uint64_t k_eps_i_arlim099_inj[2] = {0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_grad_i_arlim099_inj[2] = {0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_hess_i_arlim099_inj[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_score_i_arlim099_inj[2] = {0x8000000000000000ULL, 0x8000000000000000ULL};
static const std::uint64_t k_info_i_arlim099_inj[4] = {0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_asy_i_arlim099_inj[1] = {0};
static const std::uint64_t k_diag_i_arlim099_inj[1] = {0};
static const std::uint64_t k_nk_i_arlim099_inj_0[2] = {0x3fefd70a3d70a3d7ULL, 0x3fe0000000000000ULL};
static const Node k_nodes_i_arlim099_inj[1] = {
    {k_nk_i_arlim099_inj_0, 0x407b45acf0b239d9ULL, 0},
};
static const std::uint64_t k_point_i_eps2x[2] = {0x3fd999999999999aULL, 0x3fd999999999999aULL};
static const std::uint64_t k_yd_i_eps2x[107] = {0xbff9653f1c54b300ULL, 0x4000b0eae77cd640ULL, 0x40086ba92f652ea0ULL, 0xbff79ef720170040ULL, 0xbfeb74cde5c6dec0ULL, 0xbff756c14aac7600ULL, 0x4016bb9658f2ef68ULL, 0xbff222c93b06e520ULL, 0xbfc16763337ca600ULL, 0x3fb5684f8dad3600ULL, 0x3ff44791c01b0a40ULL, 0xbff82a00aadc3fa0ULL, 0xbff4d7a66e617440ULL, 0xbff6775e594eeba0ULL, 0x3fe7756de3f59a40ULL, 0x3ff5e6b07a652940ULL, 0x3fe84334c0c62640ULL, 0xbff83ab21d906800ULL, 0xc00c163f65d9d150ULL, 0x40033fe6fd02fd40ULL, 0xbfd22444ff2e0100ULL, 0x3ff4fecd39dfca40ULL, 0xc00532e6378e5520ULL, 0x3feadcf2d2d2bd00ULL, 0x3fe56747389f9fc0ULL, 0xbffac53930b721c0ULL, 0x400b538cf80acd30ULL, 0xc01013e52386efd8ULL, 0x4001dde828109f50ULL, 0xbff864074f2efe00ULL, 0x3ff971eb051a4820ULL, 0xc006b4e1592c3060ULL, 0x400f8949c98fec90ULL, 0xc012eeeeec20ed80ULL, 0x40152086479f97f8ULL, 0xbff2a70a19254bc0ULL, 0xbff1e1fb4a563480ULL, 0x40043a0af961dda0ULL, 0xc00f08d20322d3b0ULL, 0x4007dbc55dd319f0ULL, 0xc00b5880af4cfaf0ULL, 0x400f928c1088bf00ULL, 0x3ff65585633a8780ULL, 0xc0016e267dc0cca0ULL, 0xbfd34e47a4e26380ULL, 0x4012502e85a1cba8ULL, 0xc017271253d0d470ULL, 0xbfad2cccca79a000ULL, 0x3ff6ce83af191100ULL, 0xc01085d9fb543220ULL, 0x3f6c49e52f530000ULL, 0xbff79c4898038e40ULL, 0x3ff4c111c6501fa0ULL, 0xbff1a3f8bdadbe60ULL, 0xc014621c5d910550ULL, 0x40190665347eb4c0ULL, 0xc0059d9f198f3d10ULL, 0xc00403d9b314a3b0ULL, 0x40051ca35a1c3ee0ULL, 0xbfcb20951feece00ULL, 0xc001b8d585fa62c0ULL, 0x4012a8318f949328ULL, 0xc00bab8788f45098ULL, 0x3ffbf4525c2b0410ULL, 0x400c1b0ad789ad60ULL, 0xc00f2831bb14f310ULL, 0x401d41f6d72a30d8ULL, 0xc0074c46c9d7ab30ULL, 0x400164e4ebf8ea90ULL, 0x3ff32678e26e6fe0ULL, 0x3fbd8276a8f14000ULL, 0xbfa6368dd19c0800ULL, 0x4001e4d159ca6130ULL, 0xc00cfbb75cbce020ULL, 0x4010d9fc37bad1ccULL, 0x3fe1e3d18ce32a00ULL, 0xc006189cd0056120ULL, 0x3fce2563f578bf00ULL, 0xbf66af23bf450000ULL, 0xbffac1754fbe6ae0ULL, 0xbfe3518113180ec0ULL, 0x3ffaaf449929ac60ULL, 0xbfeaa3b93ea68d00ULL, 0x4000781f88b32850ULL, 0xc007dca89af7e740ULL, 0x3ffdf85a30336780ULL, 0x3ffca3dc9cb96140ULL, 0xbfde30486583b500ULL, 0xbfb522a4504b0e00ULL, 0x3ff77ef696ea0f00ULL, 0xc01309c91f98e2e8ULL, 0x3ff70db13c507b60ULL, 0xbfc1683e75a46400ULL, 0xbfc126f6251b3800ULL, 0xc00dc2daa627e150ULL, 0xbfd1b626abb45d00ULL, 0x3ff25ab696b398e0ULL, 0xbfe0a65033b7bf40ULL, 0xc00220922e7352d8ULL, 0xc001613fd146c200ULL, 0x401207a8737bd748ULL, 0xbfd83d960a8aa800ULL, 0x4005508a7a4b6d50ULL, 0x4000614ae12aac30ULL, 0xbff7a5748bc82f00ULL, 0xc000d5111ad856e0ULL, 0x400157f6013e29f0ULL};
static const std::uint64_t k_eps_i_eps2x[2] = {0xbe5ffffffa0bc6d1ULL, 0xbe5ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_eps2x[2] = {0x40ac1ff4113ba40aULL, 0x40aa7f6966ee2356ULL};
static const std::uint64_t k_hess_i_eps2x[4] = {0x40c4b4e59fda4c38ULL, 0x40b5b311ca0996f6ULL, 0x40b5b311ca0996f6ULL, 0x40c31040ed8c0825ULL};
static const std::uint64_t k_score_i_eps2x[2] = {0xc0563953b5b7ba08ULL, 0xc054f02e20245620ULL};
static const std::uint64_t k_info_i_eps2x[4] = {0x40705cb673aad988ULL, 0x4061258ea09138eaULL, 0x4061258ea09138eaULL, 0x406e20a649b40ed7ULL};
static const std::uint64_t k_asy_i_eps2x[4] = {0x3f764b58de9683ffULL, 0xbf696087773e1be7ULL, 0xbf696087773e1be7ULL, 0x3f783747b4e3ea6eULL};
static const std::uint64_t k_diag_i_eps2x[2] = {0x3f764b58de9683ffULL, 0x3f783747b4e3ea6eULL};
static const std::uint64_t k_nk_i_eps2x_0[2] = {0x3fd99999799999a0ULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_eps2x_1[2] = {0x3fd999999999999aULL, 0x3fd99999799999a0ULL};
static const std::uint64_t k_nk_i_eps2x_2[2] = {0x3fd99c6dad662625ULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_eps2x_3[2] = {0x3fd996c585cd0d0fULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_eps2x_4[2] = {0x3fd999999999999aULL, 0x3fd99c6dad662625ULL};
static const std::uint64_t k_nk_i_eps2x_5[2] = {0x3fd999999999999aULL, 0x3fd996c585cd0d0fULL};
static const std::uint64_t k_nk_i_eps2x_6[2] = {0x3fd99b03a37fdfdfULL, 0x3fd99b03a37fdfdfULL};
static const std::uint64_t k_nk_i_eps2x_7[2] = {0x3fd9982f8fb35355ULL, 0x3fd9982f8fb35355ULL};
static const std::uint64_t k_nk_i_eps2x_8[2] = {0x3fd9982f8fb35355ULL, 0x3fd99b03a37fdfdfULL};
static const std::uint64_t k_nk_i_eps2x_9[2] = {0x3fd99b03a37fdfdfULL, 0x3fd9982f8fb35355ULL};
static const std::uint64_t k_nk_i_eps2x_10[2] = {0x3fd999999999999aULL, 0x3fd999999999999aULL};
static const Node k_nodes_i_eps2x[11] = {
    {k_nk_i_eps2x_0, 0x40a09c25116259bbULL, 0},
    {k_nk_i_eps2x_1, 0x40a09c2512329f10ULL, 0},
    {k_nk_i_eps2x_2, 0x40a09d6366563293ULL, 0},
    {k_nk_i_eps2x_3, 0x40a09ae701f84027ULL, 0},
    {k_nk_i_eps2x_4, 0x40a09d50fc0efd51ULL, 0},
    {k_nk_i_eps2x_5, 0x40a09af968f62c05ULL, 0},
    {k_nk_i_eps2x_6, 0x40a09d5a2cad6795ULL, 0},
    {k_nk_i_eps2x_7, 0x40a09af030f35bffULL, 0},
    {k_nk_i_eps2x_8, 0x40a09c1befa5a7acULL, 0},
    {k_nk_i_eps2x_9, 0x40a09c2e58480a22ULL, 0},
    {k_nk_i_eps2x_10, 0x40a09c251f7253c1ULL, 0},
};
static const std::uint64_t k_point_i_freq4[3] = {0x3fd3333333333333ULL, 0xbfc999999999999aULL, 0x3fd999999999999aULL};
static const std::uint64_t k_yd_i_freq4[91] = {0xbfe3604790548680ULL, 0xc0072885a43ef4e0ULL, 0x4003e01c294529a0ULL, 0x400cb3f9b01f9840ULL, 0xbff46accd6e04d40ULL, 0xc016c720e959e340ULL, 0xc006d825bffb7420ULL, 0xbff3b215ce4fc300ULL, 0x3feb2f67e0970f00ULL, 0x401eec31463378d0ULL, 0xbfd7a5e5c9c87c00ULL, 0xbff3682f7257efc0ULL, 0xbffecde0de946440ULL, 0xbffefeeec4867700ULL, 0x3fe09f41d28f3e00ULL, 0xc000c302a625f160ULL, 0x401b6409410c9590ULL, 0xbff67e6fa8d14640ULL, 0x3ff474de5df737c0ULL, 0x400232c031c058e0ULL, 0xc01590fe6979b3e0ULL, 0xbfd990b10f973200ULL, 0xbfbc915c81459800ULL, 0x4001616278a0a300ULL, 0x3ff749592a423c80ULL, 0x40032b879d1afaa0ULL, 0x400883fcdc2e3de0ULL, 0xc02033cb1a019870ULL, 0x40106eca2f605ee0ULL, 0x3fe8ec22e9086680ULL, 0xc01a08c544ce0e40ULL, 0x4012cbcb34d38790ULL, 0xc00e9742f98a7c60ULL, 0xc0018f22e9c4a700ULL, 0x3fdbf571a21b4400ULL, 0xc00a4fc7c2572040ULL, 0x40107142a16d3740ULL, 0xc00f42fe2d5301e0ULL, 0xbfe0a327b5607780ULL, 0x3fea8912d8d53900ULL, 0xbff1c95cf17cc480ULL, 0x400bbea93b9521e0ULL, 0x400e90c723ad7f00ULL, 0x4011db67fc28fe90ULL, 0xc008d75a4cb31320ULL, 0x3ff3aba35dfc16c0ULL, 0xc002774149635880ULL, 0xbfeaba1ac0101d00ULL, 0x3ff8e4bee6f8ae40ULL, 0xc003a4326831cb80ULL, 0xc0060fe725ef9600ULL, 0x3ffeaa2f01793840ULL, 0xbfd92324e8daa400ULL, 0x40058e7081792f80ULL, 0x401aed1b4c5e5c40ULL, 0xc0144f53ec0c1620ULL, 0x3ffc7557dae52540ULL, 0xbfc7bb74786c1000ULL, 0xc00649388074e440ULL, 0xbfe99fdda6867800ULL, 0xc00200e8c848df80ULL, 0xbfdacacf51734c00ULL, 0xc003e59b3f4ee2c0ULL, 0x400887e404608d80ULL, 0xc012ec599e9a40d0ULL, 0xbff4815d40cb2680ULL, 0x3ffae72a8aa41480ULL, 0xc0086f1ea977dce0ULL, 0x401c9cfcb88ad580ULL, 0xbff1fb5a1c1c0800ULL, 0x40096d8ea9ad19c0ULL, 0x3ffa310bb7db41c0ULL, 0xc0040e26f5937760ULL, 0x3ffa9f015e485080ULL, 0xbfeea9a3b966ef80ULL, 0xbfd43fa764632a00ULL, 0x3fc55685bb051c00ULL, 0x3fe0627678920280ULL, 0x3fe6eef7dfd5c880ULL, 0x3ff1acd04bfa00c0ULL, 0x400c398d18f56c40ULL, 0xbfd1854151a11800ULL, 0xbff82fc987b6d880ULL, 0x3fda880cccc10e00ULL, 0xc01e623a8c50ac00ULL, 0x40079e988959ca00ULL, 0xc0038f266c349f00ULL, 0xbfef6ca950ba7900ULL, 0x401b23b44a21f660ULL, 0x3f78dd84757c8000ULL, 0x3ff2d492c4bb4b80ULL};
static const std::uint64_t k_eps_i_freq4[3] = {0xbe4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_freq4[3] = {0xc088f3cc5ca4877cULL, 0xc09443896bc534a7ULL, 0x4086d4d9343f81b5ULL};
static const std::uint64_t k_hess_i_freq4[9] = {0x40a87ec7e08ec1b6ULL, 0x408d584ec575c219ULL, 0xc0a4799fd1cf44f3ULL, 0x408d584ec575c219ULL, 0x409e4a3ab9a2c54eULL, 0xc08a3fe0e4e25120ULL, 0xc0a4799fd1cf44f3ULL, 0xc08a3fe0e4e25120ULL, 0x4099dea8f8d03a6bULL};
static const std::uint64_t k_score_i_freq4[3] = {0x403dbf5091e72925ULL, 0x404828676d9d1aa5ULL, 0xc03b38074b8139b4ULL};
static const std::uint64_t k_info_i_freq4[9] = {0x405d33cf7919f38fULL, 0x40417df54ca1df96ULL, 0xc05868e2a1952748ULL, 0x40417df54ca1df96ULL, 0x40520e29e0d90b85ULL, 0xc03f4b359ab3e80cULL, 0xc05868e2a1952748ULL, 0xc03f4b359ab3e80cULL, 0x404ed74f130ceebdULL};
static const std::uint64_t k_asy_i_freq4[9] = {0xbf98abfd14b7e9a6ULL, 0xbf797cdce85a0942ULL, 0xbfa524b1a89277eaULL, 0xbf797cdce85a0942ULL, 0x3f90873683a15b3dULL, 0xbf5b3729756015faULL, 0xbfa524b1a89277eaULL, 0xbf5b3729756015faULL, 0xbfa99973093aa018ULL};
static const std::uint64_t k_diag_i_freq4[3] = {0xbf98abfd14b7e9a6ULL, 0x3f90873683a15b3dULL, 0xbfa99973093aa018ULL};
static const std::uint64_t k_nk_i_freq4_0[3] = {0x3fd3333323333336ULL, 0xbfc999999999999aULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_1[3] = {0x3fd3333333333333ULL, 0xbfc99999799999a0ULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_2[3] = {0x3fd3333333333333ULL, 0xbfc999999999999aULL, 0x3fd999998999999dULL};
static const std::uint64_t k_nk_i_freq4_3[3] = {0x3fd3353333330391ULL, 0xbfc999999999999aULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_4[3] = {0x3fd33133333362d5ULL, 0xbfc999999999999aULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_5[3] = {0x3fd3333333333333ULL, 0xbfc995999999f8deULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_6[3] = {0x3fd3333333333333ULL, 0xbfc99d9999993a56ULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_7[3] = {0x3fd3333333333333ULL, 0xbfc999999999999aULL, 0x3fd99b99999969f8ULL};
static const std::uint64_t k_nk_i_freq4_8[3] = {0x3fd3333333333333ULL, 0xbfc999999999999aULL, 0x3fd997999999c93cULL};
static const std::uint64_t k_nk_i_freq4_9[3] = {0x3fd3343333331b62ULL, 0xbfc997999999c93cULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_10[3] = {0x3fd3323333334b04ULL, 0xbfc99b99999969f8ULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_11[3] = {0x3fd3323333334b04ULL, 0xbfc997999999c93cULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_12[3] = {0x3fd3343333331b62ULL, 0xbfc99b99999969f8ULL, 0x3fd999999999999aULL};
static const std::uint64_t k_nk_i_freq4_13[3] = {0x3fd3343333331b62ULL, 0xbfc999999999999aULL, 0x3fd99a99999981c9ULL};
static const std::uint64_t k_nk_i_freq4_14[3] = {0x3fd3323333334b04ULL, 0xbfc999999999999aULL, 0x3fd998999999b16bULL};
static const std::uint64_t k_nk_i_freq4_15[3] = {0x3fd3323333334b04ULL, 0xbfc999999999999aULL, 0x3fd99a99999981c9ULL};
static const std::uint64_t k_nk_i_freq4_16[3] = {0x3fd3343333331b62ULL, 0xbfc999999999999aULL, 0x3fd998999999b16bULL};
static const std::uint64_t k_nk_i_freq4_17[3] = {0x3fd3333333333333ULL, 0xbfc997999999c93cULL, 0x3fd99a99999981c9ULL};
static const std::uint64_t k_nk_i_freq4_18[3] = {0x3fd3333333333333ULL, 0xbfc99b99999969f8ULL, 0x3fd998999999b16bULL};
static const std::uint64_t k_nk_i_freq4_19[3] = {0x3fd3333333333333ULL, 0xbfc99b99999969f8ULL, 0x3fd99a99999981c9ULL};
static const std::uint64_t k_nk_i_freq4_20[3] = {0x3fd3333333333333ULL, 0xbfc997999999c93cULL, 0x3fd998999999b16bULL};
static const std::uint64_t k_nk_i_freq4_21[3] = {0x3fd3333333333333ULL, 0xbfc999999999999aULL, 0x3fd999999999999aULL};
static const Node k_nodes_i_freq4[22] = {
    {k_nk_i_freq4_0, 0x4092742cd4586f44ULL, 0},
    {k_nk_i_freq4_1, 0x4092742ccc29135fULL, 0},
    {k_nk_i_freq4_2, 0x4092742cce5f5a93ULL, 0},
    {k_nk_i_freq4_3, 0x409273c9082815e2ULL, 0},
    {k_nk_i_freq4_4, 0x40927490a68b397eULL, 0},
    {k_nk_i_freq4_5, 0x4092738ab8b7deb3ULL, 0},
    {k_nk_i_freq4_6, 0x409274cef14e9b6cULL, 0},
    {k_nk_i_freq4_7, 0x4092748827da50ebULL, 0},
    {k_nk_i_freq4_8, 0x409273d1811144c4ULL, 0},
    {k_nk_i_freq4_9, 0x409273a9dee0761aULL, 0},
    {k_nk_i_freq4_10, 0x409274afca5d76e8ULL, 0},
    {k_nk_i_freq4_11, 0x4092740dac3c8886ULL, 0},
    {k_nk_i_freq4_12, 0x4092744bf9565aa4ULL, 0},
    {k_nk_i_freq4_13, 0x40927428931b286aULL, 0},
    {k_nk_i_freq4_14, 0x409274310ee81ea0ULL, 0},
    {k_nk_i_freq4_15, 0x4092748c676b2b15ULL, 0},
    {k_nk_i_freq4_16, 0x409273cd44d4ebdcULL, 0},
    {k_nk_i_freq4_17, 0x409274096db5dc07ULL, 0},
    {k_nk_i_freq4_18, 0x40927450369c9f94ULL, 0},
    {k_nk_i_freq4_19, 0x409274ab8ba53a91ULL, 0},
    {k_nk_i_freq4_20, 0x409273ae1bf53d27ULL, 0},
    {k_nk_i_freq4_21, 0x4092742cd139f5b9ULL, 0},
};
static const std::uint64_t k_point_i_dim6[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_yd_i_dim6[143] = {0xbf76c4ebb754c800ULL, 0x3f8b2a10cc212500ULL, 0xbf851635709b2b80ULL, 0xbfae25947506fa00ULL, 0xbfa39a00f5183940ULL, 0x3fa367263f155aa0ULL, 0xbf80352db1db03c0ULL, 0xbfac6ed86c0d3cd0ULL, 0x3f998c96ca9b50e0ULL, 0x3f363eb5ee6cc000ULL, 0x3fb1ec9adc4b1eb0ULL, 0x3fa4c07ce1c3d1c0ULL, 0x3f9064527029ea00ULL, 0x3f83d44e3cf20200ULL, 0x3fb128c328e87de0ULL, 0x3f9c064c0f7ee5c0ULL, 0x3f92efd44e247f40ULL, 0xbfb0cc2378747c50ULL, 0xbf7ea7299d44e300ULL, 0x3f96067173e31040ULL, 0x3f795b3f95d37700ULL, 0xbf75292bcdac2700ULL, 0xbfb345b97075bfd0ULL, 0xbfa036a9e5cebc60ULL, 0x3fa7a977569902a0ULL, 0xbfa848c8e344b400ULL, 0xbfbd9a43b6905440ULL, 0x3fb584fdd25c79b0ULL, 0x3f7ca2eb9806af00ULL, 0x3fa34bb3839fb940ULL, 0x3f88d92635cfdc80ULL, 0xbf72679089233a00ULL, 0x3f74c90db4df1c00ULL, 0x3fa062cb3bb4bca0ULL, 0x3fa36c106b30d1e0ULL, 0x3f9dfd480751c280ULL, 0xbf96f9f4e6d05780ULL, 0x3fb4b72b7679c0b0ULL, 0x3fbf2eb254f45300ULL, 0xbfa750a817e4b680ULL, 0xbfb295a4fd2a7690ULL, 0xbfa29214a5fdbf20ULL, 0xbfa9a9bb73660540ULL, 0x3f933b0d0209ea80ULL, 0x3f8316e321877c00ULL, 0x3fa35de42d2ce080ULL, 0x3fb80d9b357831f0ULL, 0x3fb65aeab9d1ec40ULL, 0x3fa3444b0c9dd1c0ULL, 0xbf942bf02b74f700ULL, 0xbfb37199d27e1060ULL, 0xbfc354800361ff50ULL, 0x3f80ddbf185f9c00ULL, 0x3f9128400bfad000ULL, 0x3f953b36172f64c0ULL, 0xbfadf7eef3565d40ULL, 0xbfa034e294dceb80ULL, 0x3f9974b8e0ffc480ULL, 0xbf91b0d77a36ac00ULL, 0x3fb81948c0d89f20ULL, 0xbf96def9a0b4aa80ULL, 0x3fc17639d4c60680ULL, 0xbf85bf40c00b9f00ULL, 0x3f7d275d33143000ULL, 0xbfb8e00e2116eae0ULL, 0xbfb0e590456e3ac0ULL, 0xbfa5fec49bf6cec0ULL, 0xbfa2b0c6fd872f80ULL, 0x3fb780a4dce1a2b0ULL, 0x3f77296c4a413e00ULL, 0x3f9647d56591f600ULL, 0xbfb8d5ca1b6ec580ULL, 0xbf90067a39f24100ULL, 0xbfa333ae48a34500ULL, 0xbfbb1c2c591d7f60ULL, 0x3fc2899fb62b8680ULL, 0x3fa834a6bfd0f5c0ULL, 0x3f8f51217d57f800ULL, 0xbfb677d2a36f9160ULL, 0x3fb2fdc43df39f20ULL, 0xbfae124d6e8f5c40ULL, 0x3f55533b6b52b800ULL, 0x3fbff4fe1fd14140ULL, 0xbfa9858f473d3800ULL, 0x3fc7659c1cdd3890ULL, 0x3fb5a8eee0114740ULL, 0x3fc7b71a751c75b0ULL, 0xbfc3bd496cce9700ULL, 0xbfb4a57b9ec02c80ULL, 0xbfba8430f0863c60ULL, 0x3fc3efc990a2c630ULL, 0x3fb2d15a42832200ULL, 0xbf993f1519b32b00ULL, 0x3fbb6409717c3bc0ULL, 0xbf5d7d5ffcd77800ULL, 0x3fd338b84da2b0b8ULL, 0xbf9b33ae6813fc00ULL, 0xbfc32418b6543ee0ULL, 0xbfbfdbaa5e09eac0ULL, 0xbfc983f13f235070ULL, 0x3fa32c1b1fabfdc0ULL, 0xbf90e7c0a448bc80ULL, 0xbfb183f1b210f760ULL, 0xbfc03632dbd6e1e0ULL, 0x3f80cf491a85e400ULL, 0xbfb75c4c449423a0ULL, 0x3fc397efbff5f430ULL, 0xbf87324819334400ULL, 0xbfa8d4ed010af980ULL, 0x3fc2773e8e5ab5c0ULL, 0xbf939e6b281dbf00ULL, 0x3fcaa22974eb2440ULL, 0xbfaf09ab07715f80ULL, 0xbfc2260ca3568b80ULL, 0xbfc044a33772d110ULL, 0x3f9a095afda7f580ULL, 0x3fbe20b958893f40ULL, 0x3fd363f5991dda60ULL, 0xbfc7e2b968b66040ULL, 0x3fcc2ee4891e5fe0ULL, 0x3fc6bed834567560ULL, 0xbf703a9054a2d400ULL, 0xbfaeca3ec2094580ULL, 0xbfd8274bf1205a60ULL, 0xbfc01f71dc3f96a0ULL, 0xbf8156d98a21a800ULL, 0x3f9381752c024480ULL, 0x3faa70004bbbb440ULL, 0xbf9f5c0e5f009600ULL, 0xbfc41ef91a347200ULL, 0x3fd30ae12ecde7a0ULL, 0xbfb7331ad97dd940ULL, 0x3f90eb5b58839100ULL, 0x3fb7ab5e78c63d00ULL, 0x3fc08fb4130a8760ULL, 0x3fc6981eb90cd6c0ULL, 0xbfae0bf75dd3b100ULL, 0xbfa8eae4b86f3900ULL, 0xbfb19f7e42f05080ULL, 0xbfcbe4683a74cdc0ULL, 0xbfb5b9bee9951cc0ULL, 0x3fad0553c60e1400ULL, 0xbfc2dfdc974b6400ULL};
static const std::uint64_t k_eps_i_dim6[6] = {0xbe4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL, 0xbe4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_dim6[6] = {0xbfd0f23ad3272e7aULL, 0x3fe9208414acd979ULL, 0xbfe7bf78546b2933ULL, 0x3fd808d6f478cff0ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_hess_i_dim6[36] = {0x4006383488225c7bULL, 0xbfee7bc3cdabfcc7ULL, 0x3fe98e5f6cc14a2dULL, 0xc00eea321dc088d2ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xbfee7bc3cdabfcc7ULL, 0x40067e08462f5a7cULL, 0x3f96fb3f0446a6abULL, 0x3ffa4478d0e32bebULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x3fe98e5f6cc14a2dULL, 0x3f96fb3f0446a6abULL, 0x40070eae5c4a4465ULL, 0xbfe916e2acab0ec2ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc00eea321dc088d2ULL, 0x3ffa4478d0e32bebULL, 0xbfe916e2acab0ec2ULL, 0x4013f4a029b685f5ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_score_i_dim6[6] = {0x402b8c5e9badd10eULL, 0xc0446c7484120ff7ULL, 0x40434d7e5cc352cdULL, 0xc033892130af6245ULL, 0x8000000000000000ULL, 0x8000000000000000ULL};
static const std::uint64_t k_info_i_dim6[36] = {0x40620f7792ebfae4ULL, 0xc048c70765af042eULL, 0x4044c5bfa25aad09ULL, 0xc06920c9f9603c08ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc048c70765af042eULL, 0x4062483945bc5fb1ULL, 0x3ff2adffe572d648ULL, 0x405559c304ed4fb2ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x4044c5bfa25aad09ULL, 0x3ff2adffe572d648ULL, 0x4062bdcbf0177877ULL, 0xc04464a097cd9b36ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0xc06920c9f9603c08ULL, 0x405559c304ed4fb2ULL, 0xc04464a097cd9b36ULL, 0x407038601033d0f2ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
static const std::uint64_t k_asy_i_dim6[1] = {0};
static const std::uint64_t k_diag_i_dim6[1] = {0};
static const std::uint64_t k_nk_i_dim6_0[6] = {0x3fd3333323333336ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_1[6] = {0x3fd3333333333333ULL, 0x3fc3333313333339ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_2[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc99999799999a0ULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_3[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999998999999dULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_4[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc99999799999a0ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_5[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333313333339ULL};
static const std::uint64_t k_nk_i_dim6_6[6] = {0x3fd3353333330391ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_7[6] = {0x3fd33133333362d5ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_8[6] = {0x3fd3333333333333ULL, 0x3fc337333332d3efULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_9[6] = {0x3fd3333333333333ULL, 0x3fc32f3333339277ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_10[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc99d9999993a56ULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_11[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc995999999f8deULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_12[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd99b99999969f8ULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_13[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd997999999c93cULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_14[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc995999999f8deULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_15[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc99d9999993a56ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_16[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc337333332d3efULL};
static const std::uint64_t k_nk_i_dim6_17[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc32f3333339277ULL};
static const std::uint64_t k_nk_i_dim6_18[6] = {0x3fd3343333331b62ULL, 0x3fc3353333330391ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_19[6] = {0x3fd3323333334b04ULL, 0x3fc33133333362d5ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_20[6] = {0x3fd3323333334b04ULL, 0x3fc3353333330391ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_21[6] = {0x3fd3343333331b62ULL, 0x3fc33133333362d5ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_22[6] = {0x3fd3343333331b62ULL, 0x3fc3333333333333ULL, 0x3fc99b99999969f8ULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_23[6] = {0x3fd3323333334b04ULL, 0x3fc3333333333333ULL, 0x3fc997999999c93cULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_24[6] = {0x3fd3323333334b04ULL, 0x3fc3333333333333ULL, 0x3fc99b99999969f8ULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_25[6] = {0x3fd3343333331b62ULL, 0x3fc3333333333333ULL, 0x3fc997999999c93cULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_26[6] = {0x3fd3333333333333ULL, 0x3fc3353333330391ULL, 0x3fc99b99999969f8ULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_27[6] = {0x3fd3333333333333ULL, 0x3fc33133333362d5ULL, 0x3fc997999999c93cULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_28[6] = {0x3fd3333333333333ULL, 0x3fc33133333362d5ULL, 0x3fc99b99999969f8ULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_29[6] = {0x3fd3333333333333ULL, 0x3fc3353333330391ULL, 0x3fc997999999c93cULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_30[6] = {0x3fd3343333331b62ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd99a99999981c9ULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_31[6] = {0x3fd3323333334b04ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd998999999b16bULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_32[6] = {0x3fd3323333334b04ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd99a99999981c9ULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_33[6] = {0x3fd3343333331b62ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd998999999b16bULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_34[6] = {0x3fd3333333333333ULL, 0x3fc3353333330391ULL, 0x3fc999999999999aULL, 0x3fd99a99999981c9ULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_35[6] = {0x3fd3333333333333ULL, 0x3fc33133333362d5ULL, 0x3fc999999999999aULL, 0x3fd998999999b16bULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_36[6] = {0x3fd3333333333333ULL, 0x3fc33133333362d5ULL, 0x3fc999999999999aULL, 0x3fd99a99999981c9ULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_37[6] = {0x3fd3333333333333ULL, 0x3fc3353333330391ULL, 0x3fc999999999999aULL, 0x3fd998999999b16bULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_38[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc99b99999969f8ULL, 0x3fd99a99999981c9ULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_39[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc997999999c93cULL, 0x3fd998999999b16bULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_40[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc997999999c93cULL, 0x3fd99a99999981c9ULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_41[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc99b99999969f8ULL, 0x3fd998999999b16bULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_42[6] = {0x3fd3343333331b62ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc997999999c93cULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_43[6] = {0x3fd3323333334b04ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc99b99999969f8ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_44[6] = {0x3fd3323333334b04ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc997999999c93cULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_45[6] = {0x3fd3343333331b62ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc99b99999969f8ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_46[6] = {0x3fd3333333333333ULL, 0x3fc3353333330391ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc997999999c93cULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_47[6] = {0x3fd3333333333333ULL, 0x3fc33133333362d5ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc99b99999969f8ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_48[6] = {0x3fd3333333333333ULL, 0x3fc33133333362d5ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc997999999c93cULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_49[6] = {0x3fd3333333333333ULL, 0x3fc3353333330391ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc99b99999969f8ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_50[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc99b99999969f8ULL, 0x3fd999999999999aULL, 0xbfc997999999c93cULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_51[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc997999999c93cULL, 0x3fd999999999999aULL, 0xbfc99b99999969f8ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_52[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc997999999c93cULL, 0x3fd999999999999aULL, 0xbfc997999999c93cULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_53[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc99b99999969f8ULL, 0x3fd999999999999aULL, 0xbfc99b99999969f8ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_54[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd99a99999981c9ULL, 0xbfc997999999c93cULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_55[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd998999999b16bULL, 0xbfc99b99999969f8ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_56[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd998999999b16bULL, 0xbfc997999999c93cULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_57[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd99a99999981c9ULL, 0xbfc99b99999969f8ULL, 0x3fc3333333333333ULL};
static const std::uint64_t k_nk_i_dim6_58[6] = {0x3fd3343333331b62ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_59[6] = {0x3fd3323333334b04ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_60[6] = {0x3fd3323333334b04ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_61[6] = {0x3fd3343333331b62ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_62[6] = {0x3fd3333333333333ULL, 0x3fc3353333330391ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_63[6] = {0x3fd3333333333333ULL, 0x3fc33133333362d5ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_64[6] = {0x3fd3333333333333ULL, 0x3fc33133333362d5ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_65[6] = {0x3fd3333333333333ULL, 0x3fc3353333330391ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_66[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc99b99999969f8ULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_67[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc997999999c93cULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_68[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc997999999c93cULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_69[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc99b99999969f8ULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_70[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd99a99999981c9ULL, 0xbfc999999999999aULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_71[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd998999999b16bULL, 0xbfc999999999999aULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_72[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd998999999b16bULL, 0xbfc999999999999aULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_73[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd99a99999981c9ULL, 0xbfc999999999999aULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_74[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc997999999c93cULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_75[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc99b99999969f8ULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_76[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc99b99999969f8ULL, 0x3fc3353333330391ULL};
static const std::uint64_t k_nk_i_dim6_77[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc997999999c93cULL, 0x3fc33133333362d5ULL};
static const std::uint64_t k_nk_i_dim6_78[6] = {0x3fd3333333333333ULL, 0x3fc3333333333333ULL, 0x3fc999999999999aULL, 0x3fd999999999999aULL, 0xbfc999999999999aULL, 0x3fc3333333333333ULL};
static const Node k_nodes_i_dim6[79] = {
    {k_nk_i_dim6_0, 0x3ff511975d107fb2ULL, 0},
    {k_nk_i_dim6_1, 0x3ff5119758dd4b83ULL, 0},
    {k_nk_i_dim6_2, 0x3ff511975ef94b0fULL, 0},
    {k_nk_i_dim6_3, 0x3ff511975a80ce96ULL, 0},
    {k_nk_i_dim6_4, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_5, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_6, 0x3ff511757d19caa2ULL, 0},
    {k_nk_i_dim6_7, 0x3ff511b9460507aaULL, 0},
    {k_nk_i_dim6_8, 0x3ff511fbe3b1c438ULL, 0},
    {k_nk_i_dim6_9, 0x3ff51132df8ff7f3ULL, 0},
    {k_nk_i_dim6_10, 0x3ff5113863e3cc8bULL, 0},
    {k_nk_i_dim6_11, 0x3ff511f65fa642abULL, 0},
    {k_nk_i_dim6_12, 0x3ff511c777aa2dd4ULL, 0},
    {k_nk_i_dim6_13, 0x3ff51167544d2a5cULL, 0},
    {k_nk_i_dim6_14, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_15, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_16, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_17, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_18, 0x3ff511b8aca68530ULL, 0},
    {k_nk_i_dim6_19, 0x3ff511760f0b3e35ULL, 0},
    {k_nk_i_dim6_20, 0x3ff511da9303dff4ULL, 0},
    {k_nk_i_dim6_21, 0x3ff511542c7d5be9ULL, 0},
    {k_nk_i_dim6_22, 0x3ff51156ee76caa4ULL, 0},
    {k_nk_i_dim6_23, 0x3ff511d7d0cdafb9ULL, 0},
    {k_nk_i_dim6_24, 0x3ff51178d153863dULL, 0},
    {k_nk_i_dim6_25, 0x3ff511b5eabf2832ULL, 0},
    {k_nk_i_dim6_26, 0x3ff5119a20f7ba40ULL, 0},
    {k_nk_i_dim6_27, 0x3ff511949cc8123dULL, 0},
    {k_nk_i_dim6_28, 0x3ff511359edb511fULL, 0},
    {k_nk_i_dim6_29, 0x3ff511f91ecd801eULL, 0},
    {k_nk_i_dim6_30, 0x3ff5119e72a27c4eULL, 0},
    {k_nk_i_dim6_31, 0x3ff511904569ddd0ULL, 0},
    {k_nk_i_dim6_32, 0x3ff511c05ed2c25fULL, 0},
    {k_nk_i_dim6_33, 0x3ff5116e68aeb0cbULL, 0},
    {k_nk_i_dim6_34, 0x3ff511e1ab6bc997ULL, 0},
    {k_nk_i_dim6_35, 0x3ff5114d17ac669fULL, 0},
    {k_nk_i_dim6_36, 0x3ff5117d26122fb3ULL, 0},
    {k_nk_i_dim6_37, 0x3ff511b19674e250ULL, 0},
    {k_nk_i_dim6_38, 0x3ff5117fe90eab2aULL, 0},
    {k_nk_i_dim6_39, 0x3ff511aed5419c8aULL, 0},
    {k_nk_i_dim6_40, 0x3ff511dee8815cd7ULL, 0},
    {k_nk_i_dim6_41, 0x3ff5114fd8f1c732ULL, 0},
    {k_nk_i_dim6_42, 0x3ff511866b2a1011ULL, 0},
    {k_nk_i_dim6_43, 0x3ff511a84f9fae8bULL, 0},
    {k_nk_i_dim6_44, 0x3ff511a84f9fae8bULL, 0},
    {k_nk_i_dim6_45, 0x3ff511866b2a1011ULL, 0},
    {k_nk_i_dim6_46, 0x3ff511c99e71af4bULL, 0},
    {k_nk_i_dim6_47, 0x3ff511651c60c9c7ULL, 0},
    {k_nk_i_dim6_48, 0x3ff511651c60c9c7ULL, 0},
    {k_nk_i_dim6_49, 0x3ff511c99e71af4bULL, 0},
    {k_nk_i_dim6_50, 0x3ff51167de81a910ULL, 0},
    {k_nk_i_dim6_51, 0x3ff511c6dc62e4c6ULL, 0},
    {k_nk_i_dim6_52, 0x3ff511c6dc62e4c6ULL, 0},
    {k_nk_i_dim6_53, 0x3ff51167de81a910ULL, 0},
    {k_nk_i_dim6_54, 0x3ff511af67571710ULL, 0},
    {k_nk_i_dim6_55, 0x3ff5117f55a8c904ULL, 0},
    {k_nk_i_dim6_56, 0x3ff5117f55a8c904ULL, 0},
    {k_nk_i_dim6_57, 0x3ff511af67571710ULL, 0},
    {k_nk_i_dim6_58, 0x3ff511866b2a1011ULL, 0},
    {k_nk_i_dim6_59, 0x3ff511a84f9fae8bULL, 0},
    {k_nk_i_dim6_60, 0x3ff511a84f9fae8bULL, 0},
    {k_nk_i_dim6_61, 0x3ff511866b2a1011ULL, 0},
    {k_nk_i_dim6_62, 0x3ff511c99e71af4bULL, 0},
    {k_nk_i_dim6_63, 0x3ff511651c60c9c7ULL, 0},
    {k_nk_i_dim6_64, 0x3ff511651c60c9c7ULL, 0},
    {k_nk_i_dim6_65, 0x3ff511c99e71af4bULL, 0},
    {k_nk_i_dim6_66, 0x3ff51167de81a910ULL, 0},
    {k_nk_i_dim6_67, 0x3ff511c6dc62e4c6ULL, 0},
    {k_nk_i_dim6_68, 0x3ff511c6dc62e4c6ULL, 0},
    {k_nk_i_dim6_69, 0x3ff51167de81a910ULL, 0},
    {k_nk_i_dim6_70, 0x3ff511af67571710ULL, 0},
    {k_nk_i_dim6_71, 0x3ff5117f55a8c904ULL, 0},
    {k_nk_i_dim6_72, 0x3ff5117f55a8c904ULL, 0},
    {k_nk_i_dim6_73, 0x3ff511af67571710ULL, 0},
    {k_nk_i_dim6_74, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_75, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_76, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_77, 0x3ff511975c015c05ULL, 0},
    {k_nk_i_dim6_78, 0x3ff511975c015c05ULL, 0},
};
static const std::uint64_t k_point_i_short[2] = {0xbfe0000000000000ULL, 0xbfd3333333333333ULL};
static const std::uint64_t k_yd_i_short[17] = {0x40074515d5d80570ULL, 0x4010062d17d76e88ULL, 0xbfcd2f735cdf7500ULL, 0xbfe839f8221e47c0ULL, 0xbfd9c9c2c6e9dc80ULL, 0xbfcadf621e8c3a00ULL, 0xbfcd159cb89bc200ULL, 0xbfedb121d052efc0ULL, 0x3ffd716de343b920ULL, 0xbfd64495d17a5300ULL, 0x3ff160a8dc8e1480ULL, 0xbfe21464ab30b000ULL, 0x3fdab2aa0e78e680ULL, 0xbfa98ee828565000ULL, 0xbfb4756ca39dc600ULL, 0xbfda2ff952042f80ULL, 0x3fe88cc8b896cac0ULL};
static const std::uint64_t k_eps_i_short[2] = {0x3e4ffffffa0bc6d1ULL, 0x3e4ffffffa0bc6d1ULL};
static const std::uint64_t k_grad_i_short[2] = {0xc03a44d7dce33d9aULL, 0xc02d7d48257ca33eULL};
static const std::uint64_t k_hess_i_short[4] = {0xc02ba10bd5240734ULL, 0x4021d04503507eacULL, 0x4021d04503507eacULL, 0x4034f8e463e6f2f8ULL};
static const std::uint64_t k_score_i_short[2] = {0x401101b29f2350d6ULL, 0x4003177188cbae4cULL};
static const std::uint64_t k_info_i_short[4] = {0xc001e3204abd8d2dULL, 0x3ff710aef9b36b62ULL, 0x3ff710aef9b36b62ULL, 0x400b27b23c595f47ULL};
static const std::uint64_t k_asy_i_short[4] = {0xbfd6788ca9491cd1ULL, 0x3fc31624ae2e1e7fULL, 0x3fc31624ae2e1e7fULL, 0x3fcd9a7b2fc33c54ULL};
static const std::uint64_t k_diag_i_short[2] = {0xbfd6788ca9491cd1ULL, 0x3fcd9a7b2fc33c54ULL};
static const std::uint64_t k_nk_i_short_0[2] = {0xbfdffffff0000003ULL, 0xbfd3333333333333ULL};
static const std::uint64_t k_nk_i_short_1[2] = {0xbfe0000000000000ULL, 0xbfd3333323333336ULL};
static const std::uint64_t k_nk_i_short_2[2] = {0xbfdffe0000002fa2ULL, 0xbfd3333333333333ULL};
static const std::uint64_t k_nk_i_short_3[2] = {0xbfe000ffffffe82fULL, 0xbfd3333333333333ULL};
static const std::uint64_t k_nk_i_short_4[2] = {0xbfe0000000000000ULL, 0xbfd33133333362d5ULL};
static const std::uint64_t k_nk_i_short_5[2] = {0xbfe0000000000000ULL, 0xbfd3353333330391ULL};
static const std::uint64_t k_nk_i_short_6[2] = {0xbfdfff00000017d1ULL, 0xbfd3323333334b04ULL};
static const std::uint64_t k_nk_i_short_7[2] = {0xbfe0007ffffff418ULL, 0xbfd3343333331b62ULL};
static const std::uint64_t k_nk_i_short_8[2] = {0xbfe0007ffffff418ULL, 0xbfd3323333334b04ULL};
static const std::uint64_t k_nk_i_short_9[2] = {0xbfdfff00000017d1ULL, 0xbfd3343333331b62ULL};
static const std::uint64_t k_nk_i_short_10[2] = {0xbfe0000000000000ULL, 0xbfd3333333333333ULL};
static const Node k_nodes_i_short[11] = {
    {k_nk_i_short_0, 0x40472b5f0bf40303ULL, 0},
    {k_nk_i_short_1, 0x40472b5f0d64c97cULL, 0},
    {k_nk_i_short_2, 0x40472af5fb005374ULL, 0},
    {k_nk_i_short_3, 0x40472bc821bed7cbULL, 0},
    {k_nk_i_short_4, 0x40472b2415fc35d7ULL, 0},
    {k_nk_i_short_5, 0x40472b9a0b1c22b1ULL, 0},
    {k_nk_i_short_6, 0x40472b0d08a8d325ULL, 0},
    {k_nk_i_short_7, 0x40472bb116982defULL, 0},
    {k_nk_i_short_8, 0x40472b761b799f3dULL, 0},
    {k_nk_i_short_9, 0x40472b4802aa5d87ULL, 0},
    {k_nk_i_short_10, 0x40472b5f0f3c9dfeULL, 0},
};

inline const Case* table(std::size_t& count) {
    static const Case kCases[] = {
        {"e_m0101", 0, 1, 0, 0, 1, 1, 1, 1, 12, 2, 105, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_e_m0101, k_yd_e_m0101, k_eps_e_m0101, 0x4072d2231cfaf27aULL, k_grad_e_m0101, k_hess_e_m0101, k_score_e_m0101, k_info_e_m0101, k_asy_e_m0101, k_diag_e_m0101, k_nodes_e_m0101, 11},
        {"e_m111", 0, 1, 1, 0, 1, 0, 1, 0, 12, 2, 141, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_e_m111, k_yd_e_m111, k_eps_e_m111, 0x4077e6998b4b5cfcULL, k_grad_e_m111, k_hess_e_m111, k_score_e_m111, k_info_e_m111, k_asy_e_m111, k_diag_e_m111, k_nodes_e_m111, 11},
        {"e_q3", 0, 1, 0, 1, 2, 0, 1, 1, 4, 3, 88, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_e_q3, k_yd_e_q3, k_eps_e_q3, 0x4081bf3a7e06cfb3ULL, k_grad_e_q3, k_hess_e_q3, k_score_e_q3, k_info_e_q3, k_asy_e_q3, k_diag_e_q3, k_nodes_e_q3, 22},
        {"e_m6", 0, 1, 2, 1, 2, 1, 1, 1, 12, 6, 137, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_e_m6, k_yd_e_m6, k_eps_e_m6, 0x3fdc2e95136bae83ULL, k_grad_e_m6, k_hess_e_m6, k_score_e_m6, k_info_e_m6, k_asy_e_m6, k_diag_e_m6, k_nodes_e_m6, 79},
        {"e_short", 0, 1, 0, 0, 1, 1, 1, 1, 12, 2, 21, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_e_short, k_yd_e_short, k_eps_e_short, 0x4054bcade71f6ed7ULL, k_grad_e_short, k_hess_e_short, k_score_e_short, k_info_e_short, k_asy_e_short, k_diag_e_short, k_nodes_e_short, 11},
        {"e_qtr3", 0, 1, 1, 1, 0, 1, 1, 1, 4, 3, 88, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_e_qtr3, k_yd_e_qtr3, k_eps_e_qtr3, 0x4077d3cb2488c0eeULL, k_grad_e_qtr3, k_hess_e_qtr3, k_score_e_qtr3, k_info_e_qtr3, k_asy_e_qtr3, k_diag_e_qtr3, k_nodes_e_qtr3, 22},
        {"e_d2", 0, 1, 1, 0, 1, 0, 2, 0, 12, 2, 140, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_e_d2, k_yd_e_d2, k_eps_e_d2, 0x4075bf11f7aad7d3ULL, k_grad_e_d2, k_hess_e_d2, k_score_e_d2, k_info_e_d2, k_asy_e_d2, k_diag_e_d2, k_nodes_e_d2, 11},
        {"e_arlim099", 0, 1, 1, 0, 1, 1, 1, 1, 12, 3, 104, 1, 0x3fefae147ae147aeULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_e_arlim099, k_yd_e_arlim099, k_eps_e_arlim099, 0x4077ad072180a2e0ULL, k_grad_e_arlim099, k_hess_e_arlim099, k_score_e_arlim099, k_info_e_arlim099, k_asy_e_arlim099, k_diag_e_arlim099, k_nodes_e_arlim099, 22},
        {"i_arlim_pos", 1, 0, 1, 1, 0, 0, 1, 1, 12, 2, 105, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_arlim_pos, k_yd_i_arlim_pos, k_eps_i_arlim_pos, 0x407d08734969973aULL, k_grad_i_arlim_pos, k_hess_i_arlim_pos, k_score_i_arlim_pos, k_info_i_arlim_pos, nullptr, nullptr, k_nodes_i_arlim_pos, 6},
        {"i_arlim_neg", 1, 0, 1, 1, 0, 0, 1, 1, 12, 2, 105, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_arlim_neg, k_yd_i_arlim_neg, k_eps_i_arlim_neg, 0x409727c39fa68acfULL, k_grad_i_arlim_neg, k_hess_i_arlim_neg, k_score_i_arlim_neg, k_info_i_arlim_neg, nullptr, nullptr, k_nodes_i_arlim_neg, 6},
        {"i_ar_over_pos", 2, 0, 1, 1, 0, 0, 1, 1, 12, 2, 105, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_ar_over_pos, k_yd_i_ar_over_pos, k_eps_i_ar_over_pos, 0x0000000000000000ULL, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, k_nodes_i_ar_over_pos, 1},
        {"i_ar_over_neg", 2, 0, 1, 1, 0, 0, 1, 1, 12, 2, 105, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_ar_over_neg, k_yd_i_ar_over_neg, k_eps_i_ar_over_neg, 0x0000000000000000ULL, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, k_nodes_i_ar_over_neg, 1},
        {"i_bphi_at", 1, 0, 1, 1, 0, 0, 1, 1, 12, 2, 105, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_bphi_at, k_yd_i_bphi_at, k_eps_i_bphi_at, 0x4093eae25e523d04ULL, k_grad_i_bphi_at, k_hess_i_bphi_at, k_score_i_bphi_at, k_info_i_bphi_at, nullptr, nullptr, k_nodes_i_bphi_at, 6},
        {"i_bphi_over", 2, 0, 1, 1, 0, 0, 1, 1, 12, 2, 105, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_bphi_over, k_yd_i_bphi_over, k_eps_i_bphi_over, 0x0000000000000000ULL, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, k_nodes_i_bphi_over, 1},
        {"i_nobs0", 1, 0, 1, 1, 0, 0, 1, 1, 12, 2, 0, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_nobs0, k_yd_i_nobs0, k_eps_i_nobs0, 0x408cbabaec900657ULL, k_grad_i_nobs0, k_hess_i_nobs0, k_score_i_nobs0, k_info_i_nobs0, nullptr, nullptr, k_nodes_i_nobs0, 11},
        {"i_nobsneg", 0, 0, 1, 1, 0, 0, 1, 1, 12, 2, -5, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_nobsneg, k_yd_i_nobsneg, k_eps_i_nobsneg, 0x408cbabaec900657ULL, k_grad_i_nobsneg, k_hess_i_nobsneg, k_score_i_nobsneg, k_info_i_nobsneg, k_asy_i_nobsneg, k_diag_i_nobsneg, k_nodes_i_nobsneg, 11},
        {"i_thetab", 1, 0, 1, 0, 1, 0, 1, 0, 12, 2, 117, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_thetab, k_yd_i_thetab, k_eps_i_thetab, 0x40cd0b1d959312faULL, k_grad_i_thetab, k_hess_i_thetab, k_score_i_thetab, k_info_i_thetab, nullptr, nullptr, k_nodes_i_thetab, 6},
        {"i_thetab_off", 0, 0, 1, 0, 1, 0, 1, 0, 12, 2, 117, 0, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_thetab_off, k_yd_i_thetab_off, k_eps_i_thetab_off, 0x40cd0b1d959312faULL, k_grad_i_thetab_off, k_hess_i_thetab_off, k_score_i_thetab_off, k_info_i_thetab_off, k_asy_i_thetab_off, k_diag_i_thetab_off, k_nodes_i_thetab_off, 11},
        {"i_thetaneb", 1, 0, 1, 0, 1, 0, 1, 0, 12, 2, 117, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_thetaneb, k_yd_i_thetaneb, k_eps_i_thetaneb, 0x4078e61c8a8ded87ULL, k_grad_i_thetaneb, k_hess_i_thetaneb, k_score_i_thetaneb, k_info_i_thetaneb, nullptr, nullptr, k_nodes_i_thetaneb, 6},
        {"i_theta_over", 1, 0, 1, 0, 1, 0, 1, 0, 12, 2, 117, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_theta_over, k_yd_i_theta_over, k_eps_i_theta_over, 0x40946ffbc77a23c4ULL, k_grad_i_theta_over, k_hess_i_theta_over, k_score_i_theta_over, k_info_i_theta_over, nullptr, nullptr, k_nodes_i_theta_over, 1},
        {"i_nan_phi", 2, 0, 1, 0, 1, 0, 1, 0, 12, 2, 117, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_nan_phi, k_yd_i_nan_phi, k_eps_i_nan_phi, 0x0000000000000000ULL, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, k_nodes_i_nan_phi, 1},
        {"i_nan_both", 2, 0, 1, 0, 1, 0, 1, 0, 12, 2, 117, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_nan_both, k_yd_i_nan_both, k_eps_i_nan_both, 0x0000000000000000ULL, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, k_nodes_i_nan_both, 1},
        {"i_arlim099_inj", 1, 0, 1, 1, 0, 0, 1, 1, 12, 2, 105, 1, 0x3fefae147ae147aeULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_arlim099_inj, k_yd_i_arlim099_inj, k_eps_i_arlim099_inj, 0x407b45acf0b239d9ULL, k_grad_i_arlim099_inj, k_hess_i_arlim099_inj, k_score_i_arlim099_inj, k_info_i_arlim099_inj, nullptr, nullptr, k_nodes_i_arlim099_inj, 1},
        {"i_eps2x", 0, 0, 0, 0, 1, 1, 1, 1, 12, 2, 105, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e5ffffffa0bc6d1ULL, k_point_i_eps2x, k_yd_i_eps2x, k_eps_i_eps2x, 0x40a09c251f7253c1ULL, k_grad_i_eps2x, k_hess_i_eps2x, k_score_i_eps2x, k_info_i_eps2x, k_asy_i_eps2x, k_diag_i_eps2x, k_nodes_i_eps2x, 11},
        {"i_freq4", 0, 0, 1, 1, 1, 0, 1, 1, 4, 3, 88, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_freq4, k_yd_i_freq4, k_eps_i_freq4, 0x4092742cd139f5b9ULL, k_grad_i_freq4, k_hess_i_freq4, k_score_i_freq4, k_info_i_freq4, k_asy_i_freq4, k_diag_i_freq4, k_nodes_i_freq4, 22},
        {"i_dim6", 1, 0, 2, 1, 1, 2, 1, 1, 12, 6, 137, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_dim6, k_yd_i_dim6, k_eps_i_dim6, 0x3ff511975c015c05ULL, k_grad_i_dim6, k_hess_i_dim6, k_score_i_dim6, k_info_i_dim6, nullptr, nullptr, k_nodes_i_dim6, 79},
        {"i_short", 0, 0, 0, 0, 1, 1, 1, 1, 12, 2, 15, 1, 0x3fefffeb074a771dULL, 0x3ff0000000000000ULL, 0x3e4ffffffa0bc6d1ULL, k_point_i_short, k_yd_i_short, k_eps_i_short, 0x40472b5f0f3c9dfeULL, k_grad_i_short, k_hess_i_short, k_score_i_short, k_info_i_short, k_asy_i_short, k_diag_i_short, k_nodes_i_short, 11},
    };
    count = sizeof(kCases) / sizeof(kCases[0]);
    return kCases;
}

}  // namespace golden_fi

#endif
