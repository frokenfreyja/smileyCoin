//
// Created by Lenovo on 4/24/2020.
//

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developersg
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "serviceitemlistdb.h"

#include "base58.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "util.h"
#include <vector>

#include "init.h"
#include "main.h"
#include "sync.h"
#include "wallet.h"

inline std::string TicketAction(const CServiceTicket &ai) {return get<0>(ai);}
inline std::string TicketToAddress(const CServiceTicket &ai) {return get<1>(ai);}
inline std::string TicketLocation(const CServiceTicket &ai) {return get<2>(ai);}
inline std::string TicketName(const CServiceTicket &ai) {return get<3>(ai);}
inline std::string TicketDateAndTime(const CServiceTicket &ai) {return get<4>(ai);}
inline std::string TicketValue(const CServiceTicket &ai) {return get<5>(ai);}

inline std::string UbiAction(const CServiceUbi &ai) {return get<0>(ai);}
inline std::string UbiToAddress(const CServiceUbi &ai) {return get<1>(ai);}

inline std::string DexAction(const CServiceDex &ai) {return get<0>(ai);}
inline std::string DexToAddress(const CServiceDex &ai) {return get<1>(ai);}
inline std::string DexDescription(const CServiceDex &ai) {return get<2>(ai);}

inline std::string ChapterAction(const CServiceBook &ai) {return get<0>(ai);}
inline std::string ChapterToAddress(const CServiceBook &ai) {return get<1>(ai);}
inline std::string ChapterNum(const CServiceBook &ai) {return get<2>(ai);}

bool CServiceItemList::SetForked(const bool &fFork)
{
    fForked = fFork;
    return true;
}

bool CServiceItemList::UpdateTicketList(const std::map<std::string, std::tuple<std::string, std::string, std::string, std::string, std::string, std::string> > &map)
{
    for(std::map<std::string, std::tuple<std::string, std::string, std::string, std::string, std::string, std::string> >::const_iterator it = map.begin(); it!= map.end(); it++)
    {
        if (get<0>(it->second) == "DT" || !is_before(get<4>(it->second)) ) { // If op_return begins with DT (delete ticket)
            mapServiceTicketList::iterator itTicket = taddresses.find(it->first);
            // If key is found in ticket list
            if (itTicket != taddresses.end()) {
                taddresses.erase(itTicket);
            }
        } else if (get<0>(it->second) == "NT") { // If op_return begins with NT (new ticket)
            taddresses.insert(*it);
        }
    }
    return true;
}

// The heights need to be rolled back before new blocks are connected if any were disconnected.
// TODO: We should try to get rid of this and write the height undo information to the disk instead.
bool CServiceItemList::UpdateTicketListHeights()
{
    if(!fForked)
        return true;

    CBlockIndex* pindexSeek = mapBlockIndex.find(pcoinsTip->GetBestBlock())->second;
    if(!chainActive.Contains(pindexSeek)) {
        return false;
    }

    CBlock block;
    std::map<std::string, std::tuple<std::string, std::string, std::string, std::string, std::string, std::string> > ticketItem;
    mapServiceTicketList mforkedServiceTicketList;

    for(mapServiceTicketList::const_iterator it = taddresses.begin(); it!=taddresses.end(); it++)
    {
        if (get<0>(it->second) == "DT") { // If op_return begins with DT (delete ticket)
            mapServiceTicketList::iterator itTicket = taddresses.find(it->first);
            // If key is found in ticket list
            if (itTicket != taddresses.end()) {
                taddresses.erase(itTicket);
            }
        } else if (get<0>(it->second) == "NT") { // If op_return begins with NT (new ticket)
            taddresses.insert(*it);
        }
    }

    if(fDebug) {
        LogPrintf("%d addresses seen at fork and need to be relocated\n", mforkedServiceTicketList.size());
    }

    while(pindexSeek->pprev && !mforkedServiceTicketList.empty())
    {
        // return false;
        ReadBlockFromDisk(block,pindexSeek);
        block.BuildMerkleTree();
        BOOST_FOREACH(const CTransaction &tx, block.vtx)
        {
            for(unsigned int j = 0; j < tx.vout.size(); j++)
            {
                CScript key = tx.vout[j].scriptPubKey;
                /*mapServiceTicketList::iterator it = mforkedServiceTicketList.find(key);
                if(it == mforkedServiceTicketList.end())
                    continue;

                ticketItem.insert(std::make_pair(key, std::make_tuple(TicketToAddress(it), TicketLocation(it), TicketName(it), TicketDateAndTime(it), TicketValue(it), TicketAddress(it))));
                mforkedServiceTicketList.erase(it);*/
                if(fDebug) {
                    CTxDestination dest;
                    ExtractDestination(key, dest);
                    CBitcoinAddress addr;
                    addr.Set(dest);
                    LogPrintf("%s found at height %d\n",addr.ToString(),pindexSeek->nHeight);
                }
            }
        }

        CBlockUndo undo;
        CDiskBlockPos pos = pindexSeek ->GetUndoPos();
        if (undo.ReadFromDisk(pos, pindexSeek->pprev->GetBlockHash())) //TODO: hvenær klikkar þetta?
        {
            for (unsigned int i=0; i<undo.vtxundo.size(); i++)
            {
                for (unsigned int j=0; j<undo.vtxundo[i].vprevout.size(); j++)
                {
                    CScript key = undo.vtxundo[i].vprevout[j].txout.scriptPubKey;
                    /*mapServiceTicketList::iterator it = mforkedServiceTicketList.find(key);
                    if(it == mforkedServiceTicketList.end())
                        continue;
                    ticketItem.insert(std::make_pair(it->first, std::make_tuple(TicketToAddress(it), TicketLocation(it), TicketName(it), TicketDateAndTime(it), TicketValue(it), TicketAddress(it))));
                    mforkedServiceTicketList.erase(it);*/
                    if(fDebug) {
                        CTxDestination dest;
                        ExtractDestination(key, dest);
                        CBitcoinAddress addr;
                        addr.Set(dest);
                        LogPrintf("%s found at height %d\n",addr.ToString(),pindexSeek->nHeight);
                    }
                }
            }
        }
        else {
            LogPrintf("UpdateTicketListHeights(): Failed to read undo information\n");
            break;
        }
        pindexSeek = pindexSeek -> pprev;
    }

    bool ret;
    typedef std::pair<std::string, std::tuple<std::string, std::string, std::string, std::string, std::string, std::string> > pairType;
    BOOST_FOREACH(const pairType &pair, ticketItem) {
        ret = pcoinsTip->SetTicketList(pair.first, pair.second);
        assert(ret);
    }

    if(!UpdateTicketList(ticketItem))
        return false;
    return mforkedServiceTicketList.empty();
}


bool CServiceItemList::GetTicketList(std::multiset<std::pair<std::string, std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>>> &retset) const {
    for(std::map<std::string, std::tuple<std::string, std::string, std::string, std::string, std::string, std::string> >::const_iterator it=taddresses.begin(); it!=taddresses.end(); it++)
    {
        retset.insert(std::make_pair(it->first, std::make_tuple(get<0>(it->second), get<1>(it->second), get<2>(it->second), get<3>(it->second), get<4>(it->second), get<5>(it->second))));
    }
    return true;
}

bool CServiceItemList::IsTicket(std::string address) {
    for (std::map<std::string, std::tuple<std::string, std::string, std::string, std::string, std::string, std::string> >::const_iterator it = taddresses.begin();it != taddresses.end(); it++)
    {
        // If address found on ticket list
        if (address == it->first) {
            return true;
        }
    }
    return false;
}

bool CServiceItemList::UpdateUbiList(const std::map<std::string, std::tuple<std::string, std::string> > &map)
{
    for(std::map<std::string, std::tuple<std::string, std::string> >::const_iterator it = map.begin(); it!= map.end(); it++)
    {
        if (get<0>(it->second) == "DU") { // If op_return begins with DU (delete ubi recipient)
            mapServiceUbiList::iterator itUbi = uaddresses.find(it->first);
            // If key is found in ubi recipient list
            if (itUbi != uaddresses.end()) {
                uaddresses.erase(itUbi);
            }
        } else if (get<0>(it->second) == "NU") { // If op_return begins with NU (new ubi)
            uaddresses.insert(*it);
        }
    }
    return true;
}

// The heights need to be rolled back before new blocks are connected if any were disconnected.
// TODO: We should try to get rid of this and write the height undo information to the disk instead.
bool CServiceItemList::UpdateUbiListHeights()
{
    if(!fForked)
        return true;

    CBlockIndex* pindexSeek = mapBlockIndex.find(pcoinsTip->GetBestBlock())->second;
    if(!chainActive.Contains(pindexSeek)) {
        return false;
    }

    CBlock block;
    std::map<std::string, std::tuple<std::string, std::string> > ubiItem;
    mapServiceUbiList mforkedServiceUbiList;

    for(mapServiceUbiList::const_iterator it = uaddresses.begin(); it!=uaddresses.end(); it++)
    {
        if (get<0>(it->second) == "DU") { // If op_return begins with DU (delete ubi recipient)
            mapServiceUbiList::iterator itUbi = uaddresses.find(it->first);
            // If key is found in ubi recipient list
            if (itUbi != uaddresses.end()) {
                uaddresses.erase(itUbi);
            }
        } else if (get<0>(it->second) == "NU") { // If op_return begins with NU (new ubi recipient)
            uaddresses.insert(*it);
        }
    }

    if(fDebug) {
        LogPrintf("%d addresses seen at fork and need to be relocated\n", mforkedServiceUbiList.size());
    }

    while(pindexSeek->pprev && !mforkedServiceUbiList.empty())
    {

        // return false;

        ReadBlockFromDisk(block,pindexSeek);
        block.BuildMerkleTree();
        BOOST_FOREACH(const CTransaction &tx, block.vtx)
        {
            for(unsigned int j = 0; j < tx.vout.size(); j++)
            {
                CScript key = tx.vout[j].scriptPubKey;
                /*mapServiceUbiList::iterator it = mforkedServiceUbiList.find(key);
                if(it == mforkedServiceUbiList.end())
                    continue;

                ubiItem.insert(std::make_pair(key, std::make_tuple(UbiToAddress(it), UbiAddress(it))));
                mforkedServiceUbiList.erase(it);*/
                if(fDebug) {
                    CTxDestination dest;
                    ExtractDestination(key, dest);
                    CBitcoinAddress addr;
                    addr.Set(dest);
                    LogPrintf("%s found at height %d\n",addr.ToString(),pindexSeek->nHeight);
                }
            }
        }

        CBlockUndo undo;
        CDiskBlockPos pos = pindexSeek ->GetUndoPos();
        if (undo.ReadFromDisk(pos, pindexSeek->pprev->GetBlockHash())) //TODO: hvenær klikkar þetta?
        {
            for (unsigned int i=0; i<undo.vtxundo.size(); i++)
            {
                for (unsigned int j=0; j<undo.vtxundo[i].vprevout.size(); j++)
                {
                    CScript key = undo.vtxundo[i].vprevout[j].txout.scriptPubKey;
                    /*mapServiceUbiList::iterator it = mforkedServiceUbiList.find(key);
                    if(it == mforkedServiceUbiList.end())
                        continue;
                    ubiItem.insert(std::make_pair(it->first, std::make_tuple(UbiToAddress(it), UbiAddress(it))));
                    mforkedServiceUbiList.erase(it);*/
                    if(fDebug) {
                        CTxDestination dest;
                        ExtractDestination(key, dest);
                        CBitcoinAddress addr;
                        addr.Set(dest);
                        LogPrintf("%s found at height %d\n",addr.ToString(),pindexSeek->nHeight);
                    }
                }
            }
        }
        else {
            LogPrintf("UpdateUbiListHeights(): Failed to read undo information\n");
            break;
        }
        pindexSeek = pindexSeek -> pprev;
    }

    bool ret;
    typedef std::pair<std::string, std::tuple<std::string, std::string> > pairType;
    BOOST_FOREACH(const pairType &pair, ubiItem) {
        ret = pcoinsTip->SetUbiList(pair.first, pair.second);
        assert(ret);
    }

    if(!UpdateUbiList(ubiItem))
        return false;
    return mforkedServiceUbiList.empty();
}

bool CServiceItemList::GetUbiList(std::multiset<std::pair<std::string, std::tuple<std::string, std::string>>> &retset) const {
    for(std::map<std::string, std::tuple<std::string, std::string> >::const_iterator it=uaddresses.begin(); it!=uaddresses.end(); it++)
    {
        retset.insert(std::make_pair(it->first, std::make_tuple(get<0>(it->second), get<1>(it->second))));
    }
    return true;
}

bool CServiceItemList::UpdateDexList(const std::map<std::string, std::tuple<std::string, std::string, std::string> > &map)
{
    for(std::map<std::string, std::tuple<std::string, std::string, std::string> >::const_iterator it = map.begin(); it!= map.end(); it++)
    {
        if (get<0>(it->second) == "DD") { // If op_return begins with DD (delete dex)
            mapServiceDexList::iterator itDex = daddresses.find(it->first);
            // If key is found in dex list
            if (itDex != daddresses.end()) {
                daddresses.erase(itDex);
            }
        } else if (get<0>(it->second) == "ND") { // If op_return begins with ND (new dex)
            daddresses.insert(*it);
        }
    }
    return true;
}

// The heights need to be rolled back before new blocks are connected if any were disconnected.
// TODO: We should try to get rid of this and write the height undo information to the disk instead.
bool CServiceItemList::UpdateDexListHeights()
{
    if(!fForked)
        return true;

    CBlockIndex* pindexSeek = mapBlockIndex.find(pcoinsTip->GetBestBlock())->second;
    if(!chainActive.Contains(pindexSeek)) {
        return false;
    }

    CBlock block;
    std::map<std::string, std::tuple<std::string, std::string, std::string> > dexItem;
    mapServiceDexList mforkedServiceDexList;

    for(mapServiceDexList::const_iterator it = daddresses.begin(); it!=daddresses.end(); it++)
    {
        if (get<0>(it->second) == "DD") { // If op_return begins with DD (delete dex)
            mapServiceDexList::iterator itDex = daddresses.find(it->first);
            // If key is found in dex list
            if (itDex != daddresses.end()) {
                daddresses.erase(itDex);
            }
        } else if (get<0>(it->second) == "ND") { // If op_return begins with ND (new dex)
            daddresses.insert(*it);
        }
    }

    if(fDebug) {
        LogPrintf("%d addresses seen at fork and need to be relocated\n", mforkedServiceDexList.size());
    }

    while(pindexSeek->pprev && !mforkedServiceDexList.empty())
    {

        // return false;

        ReadBlockFromDisk(block,pindexSeek);
        block.BuildMerkleTree();
        BOOST_FOREACH(const CTransaction &tx, block.vtx)
        {
            for(unsigned int j = 0; j < tx.vout.size(); j++)
            {
                CScript key = tx.vout[j].scriptPubKey;
                /*mapServiceDexList::iterator it = mforkedServiceDexList.find(key);
                if(it == mforkedServiceDexList.end())
                    continue;

                dexItem.insert(std::make_pair(key, std::make_tuple(DexToAddress(it), DexAddress(it), DexDescription(it))));
                mforkedServiceDexList.erase(it);*/
                if(fDebug) {
                    CTxDestination dest;
                    ExtractDestination(key, dest);
                    CBitcoinAddress addr;
                    addr.Set(dest);
                    LogPrintf("%s found at height %d\n",addr.ToString(),pindexSeek->nHeight);
                }
            }
        }

        CBlockUndo undo;
        CDiskBlockPos pos = pindexSeek ->GetUndoPos();
        if (undo.ReadFromDisk(pos, pindexSeek->pprev->GetBlockHash())) //TODO: hvenær klikkar þetta?
        {
            for (unsigned int i=0; i<undo.vtxundo.size(); i++)
            {
                for (unsigned int j=0; j<undo.vtxundo[i].vprevout.size(); j++)
                {
                    CScript key = undo.vtxundo[i].vprevout[j].txout.scriptPubKey;
                    /*mapServiceDexList::iterator it = mforkedServiceDexList.find(key);
                    if(it == mforkedServiceDexList.end())
                        continue;
                    dexItem.insert(std::make_pair(it->first, std::make_tuple(DexToAddress(it), DexAddress(it), DexDescription(it))));
                    mforkedServiceDexList.erase(it);*/
                    if(fDebug) {
                        CTxDestination dest;
                        ExtractDestination(key, dest);
                        CBitcoinAddress addr;
                        addr.Set(dest);
                        LogPrintf("%s found at height %d\n",addr.ToString(),pindexSeek->nHeight);
                    }
                }
            }
        }
        else {
            LogPrintf("UpdateDexListHeights(): Failed to read undo information\n");
            break;
        }
        pindexSeek = pindexSeek -> pprev;
    }

    bool ret;
    typedef std::pair<std::string, std::tuple<std::string, std::string, std::string> > pairType;
    BOOST_FOREACH(const pairType &pair, dexItem) {
        ret = pcoinsTip->SetDexList(pair.first, pair.second);
        assert(ret);
    }

    if(!UpdateDexList(dexItem))
        return false;
    return mforkedServiceDexList.empty();
}

bool CServiceItemList::GetDexList(std::multiset<std::pair<std::string, std::tuple<std::string, std::string, std::string>>> &retset) const {
    for(std::map<std::string, std::tuple<std::string, std::string, std::string> >::const_iterator it=daddresses.begin(); it!=daddresses.end(); it++)
    {
        retset.insert(std::make_pair(it->first, std::make_tuple(get<0>(it->second), get<1>(it->second), get<2>(it->second))));
    }
    return true;
}

bool CServiceItemList::UpdateBookList(const std::map<std::string, std::tuple<std::string, std::string, std::string> > &map)
{
    for(std::map<std::string, std::tuple<std::string, std::string, std::string> >::const_iterator it = map.begin(); it!= map.end(); it++)
    {
        if (get<0>(it->second) == "DB") { // If op_return begins with DB (delete book chapter)
            mapServiceBookList::iterator itChapter = baddresses.find(it->first);
            // If key is found in book chapter list
            if (itChapter != baddresses.end()) {
                baddresses.erase(itChapter);
            }
        } else if (get<0>(it->second) == "NB") { // If op_return begins with NB (new book chapter)
            baddresses.insert(*it);
        }
    }
    return true;
}

// The heights need to be rolled back before new blocks are connected if any were disconnected.
// TODO: We should try to get rid of this and write the height undo information to the disk instead.
bool CServiceItemList::UpdateBookListHeights()
{
    if(!fForked)
        return true;

    CBlockIndex* pindexSeek = mapBlockIndex.find(pcoinsTip->GetBestBlock())->second;
    if(!chainActive.Contains(pindexSeek)) {
        return false;
    }

    CBlock block;
    std::map<std::string, std::tuple<std::string, std::string, std::string> > bookItem;
    mapServiceBookList mforkedServiceBookList;

    for(mapServiceBookList::const_iterator it = baddresses.begin(); it!=baddresses.end(); it++)
    {
        if (get<0>(it->second) == "DB") { // If op_return begins with DB (delete book chapter)
            mapServiceBookList::iterator itChapter = baddresses.find(it->first);
            // If key is found in book chapter list
            if (itChapter != baddresses.end()) {
                baddresses.erase(itChapter);
            }
        } else if (get<0>(it->second) == "NB") { // If op_return begins with NB (new book chapter)
            baddresses.insert(*it);
        }
    }

    if(fDebug) {
        LogPrintf("%d addresses seen at fork and need to be relocated\n", mforkedServiceBookList.size());
    }

    while(pindexSeek->pprev && !mforkedServiceBookList.empty())
    {

        // return false;

        ReadBlockFromDisk(block,pindexSeek);
        block.BuildMerkleTree();
        BOOST_FOREACH(const CTransaction &tx, block.vtx)
        {
            for(unsigned int j = 0; j < tx.vout.size(); j++)
            {
                CScript key = tx.vout[j].scriptPubKey;
                /*mapServiceBookList::iterator it = mforkedServiceBookList.find(key);
                if(it == mforkedServiceBookList.end())
                    continue;

                bookItem.insert(std::make_pair(key, std::make_tuple(BookToAddress(it), BookChapter(it))));
                mforkedServiceBookList.erase(it);*/
                if(fDebug) {
                    CTxDestination dest;
                    ExtractDestination(key, dest);
                    CBitcoinAddress addr;
                    addr.Set(dest);
                    LogPrintf("%s found at height %d\n",addr.ToString(),pindexSeek->nHeight);
                }
            }
        }

        CBlockUndo undo;
        CDiskBlockPos pos = pindexSeek ->GetUndoPos();
        if (undo.ReadFromDisk(pos, pindexSeek->pprev->GetBlockHash())) //TODO: hvenær klikkar þetta?
        {
            for (unsigned int i=0; i<undo.vtxundo.size(); i++)
            {
                for (unsigned int j=0; j<undo.vtxundo[i].vprevout.size(); j++)
                {
                    CScript key = undo.vtxundo[i].vprevout[j].txout.scriptPubKey;
                    /*mapServiceBookList::iterator it = mforkedServiceBookList.find(key);
                    if(it == mforkedServiceBookList.end())
                        continue;
                    bookItem.insert(std::make_pair(it->first, std::make_tuple(BookToAddress(it), BookChapter(it))));
                    mforkedServiceBookList.erase(it);*/
                    if(fDebug) {
                        CTxDestination dest;
                        ExtractDestination(key, dest);
                        CBitcoinAddress addr;
                        addr.Set(dest);
                        LogPrintf("%s found at height %d\n",addr.ToString(),pindexSeek->nHeight);
                    }
                }
            }
        }
        else {
            LogPrintf("UpdateBookListHeights(): Failed to read undo information\n");
            break;
        }
        pindexSeek = pindexSeek -> pprev;
    }

    bool ret;
    typedef std::pair<std::string, std::tuple<std::string, std::string, std::string> > pairType;
    BOOST_FOREACH(const pairType &pair, bookItem) {
        ret = pcoinsTip->SetBookList(pair.first, pair.second);
        assert(ret);
    }

    if(!UpdateBookList(bookItem))
        return false;
    return mforkedServiceBookList.empty();
}

bool CServiceItemList::GetBookList(std::multiset<std::pair<std::string, std::tuple<std::string, std::string, std::string>>> &retset) const {
    for(std::map<std::string, std::tuple<std::string, std::string, std::string> >::const_iterator it=baddresses.begin(); it!=baddresses.end(); it++)
    {
        retset.insert(std::make_pair(it->first, std::make_tuple(get<0>(it->second), get<1>(it->second), get<2>(it->second))));
    }
    return true;
}