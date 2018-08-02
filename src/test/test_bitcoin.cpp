// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "crypto/common.h"

#include "key.h"
#include "main.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "rpc/server.h"
#include "rpc/register.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet/db.h"
#include "wallet/wallet.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

#include "librustzcash.h"

CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h
CWallet* pwalletMain;
ZCJoinSplit *pzcashParams;

extern bool fPrintToConsole;
extern void noui_connect();

JoinSplitTestingSetup::JoinSplitTestingSetup()
{
    boost::filesystem::path pk_path = ZC_GetParamsDir() / "sprout-proving.key";
    boost::filesystem::path vk_path = ZC_GetParamsDir() / "sprout-verifying.key";
    pzcashParams = ZCJoinSplit::Prepared(vk_path.string(), pk_path.string());

    boost::filesystem::path sapling_spend = ZC_GetParamsDir() / "sapling-spend-testnet.params";
    boost::filesystem::path sapling_output = ZC_GetParamsDir() / "sapling-output-testnet.params";
    boost::filesystem::path sprout_groth16 = ZC_GetParamsDir() / "sprout-groth16-testnet.params";

    std::string sapling_spend_str = sapling_spend.string();
    std::string sapling_output_str = sapling_output.string();
    std::string sprout_groth16_str = sprout_groth16.string();

    librustzcash_init_zksnark_params(
        sapling_spend_str.c_str(),
        "35f6afd7d7514531aaa9fa529bdcddf116865f02abdd42164322bb1949227d82bdae295cad9c7b98d4bbbb00e045fa17aca79c90f53433a66bce4e82b6a1936d",
        sapling_output_str.c_str(),
        "f9d0b98ea51830c4974878f1b32bb68b2bf530e2e0ae09cd2a9b609d6fda37f1a1928e2d1ca91c31835c75dcc16057db53a807cc5cb37ebcfb753aa843a8ac21",
        sprout_groth16_str.c_str(),
        "7a6723311162cb0c664c742d2fa42278195ade98ba3f21ef4fa02b82c83aed696e107e389ac7b3b0f33f417aeefe5be775d117910a473a422b4a1b97489fbdd6"
    );
}

JoinSplitTestingSetup::~JoinSplitTestingSetup()
{
    delete pzcashParams;
}

BasicTestingSetup::BasicTestingSetup()
{
    assert(init_and_check_sodium() != -1);
    ECC_Start();
    SetupEnvironment();
    fPrintToDebugLog = false; // don't want to write to debug.log file
    fCheckBlockIndex = true;
    SelectParams(CBaseChainParams::MAIN);
}
BasicTestingSetup::~BasicTestingSetup()
{
    ECC_Stop();
}

TestingSetup::TestingSetup()
{
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.
        RegisterAllCoreRPCCommands(tableRPC);
#ifdef ENABLE_WALLET
        bitdb.MakeMock();
        RegisterWalletRPCCommands(tableRPC);
#endif
        ClearDatadirCache();
        pathTemp = GetTempPath() / strprintf("test_bitcoin_%lu_%i", (unsigned long)GetTime(), (int)(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        InitBlockIndex();
#ifdef ENABLE_WALLET
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        RegisterValidationInterface(pwalletMain);
#endif
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
        UnregisterNodeSignals(GetNodeSignals());
        threadGroup.interrupt_all();
        threadGroup.join_all();
#ifdef ENABLE_WALLET
        UnregisterValidationInterface(pwalletMain);
        delete pwalletMain;
        pwalletMain = NULL;
#endif
        UnloadBlockIndex();
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;
#ifdef ENABLE_WALLET
        bitdb.Flush(true);
        bitdb.Reset();
#endif
        boost::filesystem::remove_all(pathTemp);
}


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(CMutableTransaction &tx, CTxMemPool *pool) {
    return CTxMemPoolEntry(tx, nFee, nTime, dPriority, nHeight,
                           pool ? pool->HasNoInputsOf(tx) : hadNoDependencies,
                           spendsCoinbase, nBranchId);
}

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

bool ShutdownRequested()
{
  return false;
}
