////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011-2013, Alan C. Reiner    <alan.reiner@gmail.com>        //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE or http://www.gnu.org/licenses/agpl.html                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _BLOCKUTILS_H_
#define _BLOCKUTILS_H_

#include <stdio.h>
#include <iostream>
//#ifdef WIN32
//#include <cstdint>
//#else
//#include <stdlib.h>
//#include <inttypes.h>
//#include <cstring>
//#endif
#include <fstream>
#include <vector>
#include <queue>
#include <deque>
#include <list>
#include <bitset>
#include <map>
#include <set>
#include <limits>

#include "BinaryData.h"
#include "BtcUtils.h"
#include "BlockObj.h"

#include "cryptlib.h"
#include "sha.h"
#include "UniversalTimer.h"
#include "leveldb/db.h"


using namespace std;

class BlockDataManager_LevelDB;



////////////////////////////////////////////////////////////////////////////////
//
// LedgerEntry  
//
// LedgerEntry class is used for bother ScrAddresses and BtcWallets.  Members
// have slightly different meanings (or irrelevant) depending which one it's
// used with.
//
//  ScrAddress -- Each entry corresponds to ONE TxIn OR ONE TxOut
//
//    addr20_    -  useless - just repeating this address
//    value_     -  net debit/credit on addr balance, in Satoshis (1e-8 BTC)
//    blockNum_  -  block height of the tx in which this txin/out was included
//    txHash_    -  hash of the tx in which this txin/txout was included
//    index_     -  index of the txin/txout in this tx
//    isValid_   -  default to true -- invalidated due to reorg/double-spend
//    isCoinbase -  is the input side a coinbase/generation input
//    isSentToSelf_ - if this is a txOut, did it come from ourself?
//    isChangeBack_ - meaningless:  can't quite figure out how to determine
//                    this unless I do a prescan to determine if all txOuts
//                    are ours, or just some of them
//
//  BtcWallet -- Each entry corresponds to ONE WHOLE TRANSACTION
//
//    addr20_    -  useless - originally had a purpose, but lost it
//    value_     -  total debit/credit on WALLET balance, in Satoshis (1e-8 BTC)
//    blockNum_  -  block height of the block in which this tx was included
//    txHash_    -  hash of this tx 
//    index_     -  index of the tx in the block
//    isValid_   -  default to true -- invalidated due to reorg/double-spend
//    isCoinbase -  is the input side a coinbase/generation input
//    isSentToSelf_ - if we supplied inputs and rx ALL outputs
//    isChangeBack_ - if we supplied inputs and rx ANY outputs
//
////////////////////////////////////////////////////////////////////////////////
class LedgerEntry
{
public:
   LedgerEntry(void) :
      addr20_(0),
      value_(0),
      blockNum_(UINT32_MAX),
      txHash_(BtcUtils::EmptyHash_),
      index_(UINT32_MAX),
      txTime_(0),
      isValid_(false),
      isCoinbase_(false),
      isSentToSelf_(false),
      isChangeBack_(false) {}

   LedgerEntry(BinaryData const & addr20,
               int64_t val, 
               uint32_t blkNum, 
               BinaryData const & txhash, 
               uint32_t idx,
               uint64_t txtime=0,
               bool isCoinbase=false,
               bool isToSelf=false,
               bool isChange=false) :
      addr20_(addr20),
      value_(val),
      blockNum_(blkNum),
      txHash_(txhash),
      index_(idx),
      txTime_(txtime),
      isValid_(true),
      isCoinbase_(isCoinbase),
      isSentToSelf_(isToSelf),
      isChangeBack_(isChange) {}

   BinaryData const &  getAddrStr20(void) const { return addr20_;        }
   int64_t             getValue(void) const     { return value_;         }
   uint32_t            getBlockNum(void) const  { return blockNum_;      }
   BinaryData const &  getTxHash(void) const    { return txHash_;        }
   uint32_t            getIndex(void) const     { return index_;         }
   uint32_t            getTxTime(void) const    { return txTime_;        }
   bool                isValid(void) const      { return isValid_;       }
   bool                isCoinbase(void) const   { return isCoinbase_;    }
   bool                isSentToSelf(void) const { return isSentToSelf_;  }
   bool                isChangeBack(void) const { return isChangeBack_;  }

   void setAddr20(BinaryData const & bd) { addr20_.copyFrom(bd); }
   void setValid(bool b=true) { isValid_ = b; }
   void changeBlkNum(uint32_t newHgt) {blockNum_ = newHgt; }
      
   bool operator<(LedgerEntry const & le2) const;
   bool operator==(LedgerEntry const & le2) const;

   void pprint(void);
   void pprintOneLine(void);

private:
   

   BinaryData       addr20_;
   int64_t          value_;
   uint32_t         blockNum_;
   BinaryData       txHash_;
   uint32_t         index_;  // either a tx index, txout index or txin index
   uint64_t         txTime_;
   bool             isValid_;
   bool             isCoinbase_;
   bool             isSentToSelf_;
   bool             isChangeBack_;;


   
}; 



////////////////////////////////////////////////////////////////////////////////
class AddressBookEntry
{
public:

   /////
   AddressBookEntry(void) : addr160_(BtcUtils::EmptyHash_) { txList_.clear(); }
   AddressBookEntry(BinaryData a160) : addr160_(a160) { txList_.clear(); }
   void addTx(Tx & tx) { txList_.push_back( RegisteredTx(tx) ); }
   BinaryData getAddr160(void) { return addr160_; }

   /////
   vector<RegisteredTx> getTxList(void)
   { 
      sort(txList_.begin(), txList_.end()); 
      return txList_;
   }

   /////
   bool operator<(AddressBookEntry const & abe2) const
   {
      // If one of the entries has no tx (this shouldn't happen), sort by hash
      if( txList_.size()==0 || abe2.txList_.size()==0)
         return addr160_ < abe2.addr160_;

      return (txList_[0] < abe2.txList_[0]);
   }

private:
   BinaryData addr160_;
   vector<RegisteredTx> txList_;
};


class BtcWallet;

////////////////////////////////////////////////////////////////////////////////
//
// ScrAddress  
//
// This class is only for scanning the blockchain (information only).  It has
// no need to keep track of the public and private keys of various addresses,
// which is done by the python code leveraging this class.
//
// I call these as "scraddresses".  In most contexts, it represents an
// "address" that people use to send coins per-to-person, but it could actually
// represent any kind of TxOut script.  Multisig, P2SH, or any non-standard,
// unusual, escrow, whatever "address."  While it might be more technically
// correct to just call this class "Script" or "TxOutScript", I felt like 
// "address" is a term that will always exist in the Bitcoin ecosystem, and 
// frequently used even when not preferred.
//
// Similarly, we refer to the member variable scraddr_ as a "scradder".  It
// is actually a reduction of the TxOut script to a form that is identical
// regardless of whether pay-to-pubkey or pay-to-pubkey-hash is used. 
//
//
////////////////////////////////////////////////////////////////////////////////
class ScrAddress
{
   friend class BtcWallet;
public:

   ScrAddress(void) : 
      scrAddr_(0), firstBlockNum_(0), firstTimestamp_(0), 
      lastBlockNum_(0), lastTimestamp_(0), 
      relevantTxIOPtrs_(0), ledger_(0) {}

   ScrAddress(BinaryData    addr, 
              uint32_t      firstBlockNum  = UINT32_MAX,
              uint32_t      firstTimestamp = UINT32_MAX,
              uint32_t      lastBlockNum   = 0,
              uint32_t      lastTimestamp  = 0);
   
   BinaryData const &  getScrAddr(void) const    {return scrAddr_;      }
   uint32_t       getFirstBlockNum(void) const   {return firstBlockNum_;  }
   uint32_t       getFirstTimestamp(void) const  {return firstTimestamp_; }
   uint32_t       getLastBlockNum(void)          {return lastBlockNum_;   }
   uint32_t       getLastTimestamp(void)         {return lastTimestamp_;  }
   void           setFirstBlockNum(uint32_t b)   { firstBlockNum_  = b; }
   void           setFirstTimestamp(uint32_t t)  { firstTimestamp_ = t; }
   void           setLastBlockNum(uint32_t b)    { lastBlockNum_   = b; }
   void           setLastTimestamp(uint32_t t)   { lastTimestamp_  = t; }

   void           setScrAddr(BinaryData bd)    { scrAddr_.copyFrom(bd);}

   void     sortLedger(void);
   uint32_t removeInvalidEntries(void);

   // BlkNum is necessary for "unconfirmed" list, since it is dependent
   // on number of confirmations.  But for "spendable" TxOut list, it is
   // only a convenience, if you want to be able to calculate numConf from
   // the Utxos in the list.  If you don't care (i.e. you only want to 
   // know what TxOuts are available to spend, you can pass in 0 for currBlk
   uint64_t getFullBalance(void);
   uint64_t getSpendableBalance(uint32_t currBlk=0);
   uint64_t getUnconfirmedBalance(uint32_t currBlk);
   vector<UnspentTxOut> getFullTxOutList(uint32_t currBlk=0);
   vector<UnspentTxOut> getSpendableTxOutList(uint32_t currBlk=0);
   void clearZeroConfPool(void);


   vector<LedgerEntry> & getTxLedger(void)       { return ledger_;   }
   vector<LedgerEntry> & getZeroConfLedger(void) { return ledgerZC_; }

   vector<TxIOPair*> &   getTxIOList(void) { return relevantTxIOPtrs_; }

   void addTxIO(TxIOPair * txio, bool isZeroConf=false);
   void addTxIO(TxIOPair & txio, bool isZeroConf=false);
   void addLedgerEntry(LedgerEntry const & le, bool isZeroConf=false); 

   void pprintLedger(void);
   void clearBlkData(void);

   

private:
   BinaryData     scrAddr_; // this includes the prefix byte!
   uint32_t       firstBlockNum_;
   uint32_t       firstTimestamp_;
   uint32_t       lastBlockNum_;
   uint32_t       lastTimestamp_;

   // If any multisig scripts that include this address, we'll track them
   bool           hasMultisigEntries_;

   // Each address will store a list of pointers to its transactions
   vector<TxIOPair*>     relevantTxIOPtrs_;
   vector<TxIOPair*>     relevantTxIOPtrsZC_;
   vector<LedgerEntry>   ledger_;
   vector<LedgerEntry>   ledgerZC_;

   // Used to be part of the RegisteredAddress class
   uint32_t alreadyScannedUpToBlk_;
};




////////////////////////////////////////////////////////////////////////////////
//
// BtcWallet
//
////////////////////////////////////////////////////////////////////////////////
class BtcWallet
{
public:
   BtcWallet(void) : bdmPtr_(NULL) {}
   BtcWallet(BlockDataManager_LevelDB* bdm) : bdmPtr_(bdm) {}
   ~BtcWallet(void);

   /////////////////////////////////////////////////////////////////////////////
   // addAddress when blockchain rescan req'd, addNewAddress for just-created
   void addNewAddress(BinaryData addr);
   void addAddress(ScrAddress const & newAddr);
   void addAddress(BinaryData    addr, 
                   uint32_t      firstTimestamp = 0,
                   uint32_t      firstBlockNum  = 0,
                   uint32_t      lastTimestamp  = 0,
                   uint32_t      lastBlockNum   = 0);

   // SWIG has some serious problems with typemaps and variable arg lists
   // Here I just create some extra functions that sidestep all the problems
   // but it would be nice to figure out "typemap typecheck" in SWIG...
   void addAddress_ScrAddress_(ScrAddress const & newAddr);

   // Adds a new address that is assumed to be imported, and thus will
   // require a blockchain scan
   void addAddress_1_(BinaryData    addr);

   // Adds a new address that we claim has never been seen until thos moment,
   // and thus there's no point in doing a blockchain rescan.
   void addNewAddress_1_(BinaryData    addr) {addNewAddress(addr);}

   // Blockchain rescan will depend on the firstBlockNum input
   void addAddress_3_(BinaryData    addr, 
                      uint32_t      firstTimestamp,
                      uint32_t      firstBlockNum);

   // Blockchain rescan will depend on the firstBlockNum input
   void addAddress_5_(BinaryData    addr, 
                      uint32_t      firstTimestamp,
                      uint32_t      firstBlockNum,
                      uint32_t      lastTimestamp,
                      uint32_t      lastBlockNum);

   bool hasAddr(BinaryData const & addr20);


   // Scan a Tx for our TxIns/TxOuts.  Override default blk vals if you think
   // you will save time by not checking addresses that are much newer than
   // the block
   pair<bool,bool> isMineBulkFilter( Tx & tx );

   void       scanTx(Tx & tx, 
                     uint32_t txIndex = UINT32_MAX,
                     uint32_t blktime = UINT32_MAX,
                     uint32_t blknum  = UINT32_MAX);

   void       scanNonStdTx(uint32_t    blknum, 
                           uint32_t    txidx, 
                           Tx &        txref,
                           uint32_t    txoutidx,
                           ScrAddress& addr);

   LedgerEntry calcLedgerEntryForTx(Tx & tx);
   LedgerEntry calcLedgerEntryForTx(TxRef & txref);
   LedgerEntry calcLedgerEntryForTxStr(BinaryData txStr);

   // BlkNum is necessary for "unconfirmed" list, since it is dependent
   // on number of confirmations.  But for "spendable" TxOut list, it is
   // only a convenience, if you want to be able to calculate numConf from
   // the Utxos in the list.  If you don't care (i.e. you only want to 
   // know what TxOuts are available to spend, you can pass in 0 for currBlk
   uint64_t getFullBalance(void);
   uint64_t getSpendableBalance(uint32_t currBlk=0);
   uint64_t getUnconfirmedBalance(uint32_t currBlk);
   vector<UnspentTxOut> getFullTxOutList(uint32_t currBlk=0);
   vector<UnspentTxOut> getSpendableTxOutList(uint32_t currBlk=0);
   void clearZeroConfPool(void);

   
   uint32_t     getNumScrAddr(void) const {return scrAddrMap_.size();}
   ScrAddress & getScrAddrByIndex(uint32_t i) { return *(addrPtrVect_[i]); }
   ScrAddress & getScrAddrByKey(BinaryData const & a) { return scrAddrMap_[a];}

   void     sortLedger(void);
   uint32_t removeInvalidEntries(void);

   vector<LedgerEntry> &     getZeroConfLedger(BinaryData const * addr160=NULL);
   vector<LedgerEntry> &     getTxLedger(BinaryData const * addr160=NULL); 
   map<OutPoint, TxIOPair> & getTxIOMap(void)    {return txioMap_;}
   map<OutPoint, TxIOPair> & getNonStdTxIO(void) {return nonStdTxioMap_;}

   bool isOutPointMine(BinaryData const & hsh, uint32_t idx);

   void pprintLedger(void);
   void pprintAlot(uint32_t topBlk=0, bool withAddr=false);

   void setBdmPtr(BlockDataManager_LevelDB * bdmptr) {bdmPtr_=bdmptr;}
   void clearBlkData(void);
   
   vector<AddressBookEntry> createAddressBook(void);

   vector<LedgerEntry> & getEmptyLedger(void) { EmptyLedger_.clear(); return EmptyLedger_;}

private:
   vector<ScrAddress*>          addrPtrVect_;
   //map<BinaryData, ScrAddress>  scrAddrMap_;
   //map<OutPoint, TxIOPair>      txioMap_;

   vector<LedgerEntry>          ledgerAllAddr_;  
   vector<LedgerEntry>          ledgerAllAddrZC_;  

   // For non-std transactions
   map<OutPoint, TxIOPair>      nonStdTxioMap_;
   set<OutPoint>                nonStdUnspentOutPoints_;

   BlockDataManager_LevelDB*    bdmPtr_;
   static vector<LedgerEntry>   EmptyLedger_;


};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct ZeroConfData
{
   Tx            txobj_;   
   uint64_t      txtime_;
   list<BinaryData>::iterator iter_;
};



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// This class is a singleton -- there can only ever be one, accessed through
// the static method GetInstance().  This method gets the single instantiation
// of the BDM class, and then its public members can be used to access the 
// block data that is sitting in memory.
//
typedef enum
{
   BDM_MODE_FULL_BLOCKCHAIN,
   BDM_MODE_LIGHT_STORAGE,
   BDM_MODE_NO_STORAGE,
   BDM_MODE_COUNT
}  BDM_MODE;


typedef enum
{
  ADD_BLOCK_SUCCEEDED,
  ADD_BLOCK_NEW_TOP_BLOCK,
  ADD_BLOCK_CAUSED_REORG,
} ADD_BLOCK_RESULT_INDEX;



class BlockDataManager_LevelDB;



////////////////////////////////////////////////////////////////////////////////
//
// BlockDataManager is a SINGLETON:  only one is ever created.  
//
// Access it via BlockDataManager_LevelDB::GetInstance();
//
////////////////////////////////////////////////////////////////////////////////
class BlockDataManager_LevelDB
{
private:
   
 
   bool checkLdbStatus(leveldb::Status stat);

   bool initializeDBInterface(ARMORY_DB_TYPE dbt, DB_PRUNE_TYPE prt);

   // The header data includes file pointers to where the blocks are located.
   // If the blk files exist, but are different for some reason (moved Armory
   // to a different system), then the databases need to be rebuilt
   bool     rebuildDatabases(uint32_t startAtBlk=0);

   map<HashString, BlockHeader> headerMap_;

   //multimap<HashString, TxRef>        txHintMap_;


   // This is our permanent link to the two databases used
   InterfaceToLDB* iface_;

   
   // Need a separate memory pool just for zero-confirmation transactions
   // We need the second map to make sure we can find the data to remove
   // it, when necessary
   list<BinaryData>                   zeroConfRawTxList_;
   map<HashString, ZeroConfData>      zeroConfMap_;
   bool                               zcEnabled_;
   string                             zcFilename_;

   // This is for detecting external changes made to the blk0001.dat file
   bool                               isNetParamsSet_;
   bool                               isBlkParamsSet_;
   bool                               isLevelDBSet_;
   string                             armoryHomeDir_;
   string                             blkFileDir_;
   uint32_t                           blkFileDigits_;
   uint32_t                           blkFileStart_;
   vector<string>                     blkFileList_;
   uint64_t                           numBlkFiles_;
   uint64_t                           endOfPrevLastBlock_;

   // These should be set after the blockchain is organized
   deque<BlockHeader*>                headersByHeight_;
   BlockHeader*                       topBlockPtr_;
   BlockHeader*                       genBlockPtr_;
   uint32_t                           lastTopBlock_;

   // Reorganization details
   bool                               lastBlockWasReorg_;
   BlockHeader*                       reorgBranchPoint_;
   BlockHeader*                       prevTopBlockPtr_;
   set<HashString>                    txJustInvalidated_;
   set<HashString>                    txJustAffected_;

   // Store info on orphan chains
   vector<BlockHeader*>               previouslyValidBlockHeaderPtrs_;
   vector<BlockHeader*>               orphanChainStartBlocks_;

   static BlockDataManager_LevelDB*   theOnlyBDM_;
   static bool                        bdmCreatedYet_;
   bool                               isInitialized_;


   // These will be set for the specific network we are testing
   BinaryData GenesisHash_;
   BinaryData GenesisTxHash_;
   BinaryData MagicBytes_;
   
  
   // Variables that will be updated as the blockchain loads:
   // can be used to report load progress
   uint64_t totalBlockchainBytes_;
   uint64_t bytesReadSoFar_;
   uint32_t blocksReadSoFar_;
   uint16_t filesReadSoFar_;


   // If the BDM is not in super-node mode, then it will be specifically tracking
   // a set of addresses & wallets.  We register those addresses and wallets so
   // that we know what TxOuts to track as we process blockchain data.  And when
   // it may be necessary to do rescans.
   //
   // If instead we ARE in ARMORY_DB_SUPER (not implemented yet, as of this
   // comment being written), then we don't have anything to track -- the DB
   // will automatically update for all addresses, period.  And we'd best not 
   // track those in RAM (maybe on a huge server...?)
   set<BtcWallet*>                    registeredWallets_;
   map<BinaryData, ScrAddress>        registeredScrAddrMap_;
   list<RegisteredTx>                 registeredTxList_;
   set<HashString>                    registeredTxSet_;
   set<OutPoint>                      registeredOutPoints_;
   uint32_t                           allScannedUpToBlk_; // one past top

   map<OutPoint,   TxIOPair>          txioMap_;

private:
   // Set the constructor to private so that only one can ever be created
   BlockDataManager_LevelDB(void);
   ~BlockDataManager_LevelDB(void);

   // These are private because from outside BDM you should never call these
   // methods yourself.  You only add and remove blocks, which will call
   // these methods in the correct order.
   bool addTxToDB(StoredTx & stx);
   bool reverseTxInDB(StoredTx & stx);

public:

   static BlockDataManager_LevelDB & GetInstance(void);
   bool isInitialized(void) const { return isInitialized_;}

   void SetHomeDirLocation(string homeDir);
   bool SetBlkFileLocation(string   blkdir,
                           uint32_t blkdigits,
                           uint32_t blkstartidx,
                           uint64_t cacheSize=DEFAULT_CACHE_SIZE);
   void SetLevelDBPaths(string headerPath,
                        string txHintPath,
                        string transientPath);
   void SetBtcNetworkParams( BinaryData const & GenHash,
                             BinaryData const & GenTxHash,
                             BinaryData const & MagicBytes);
   void SelectNetwork(string netName);

   BinaryData getGenesisHash(void)   { return GenesisHash_;   }
   BinaryData getGenesisTxHash(void) { return GenesisTxHash_; }
   BinaryData getMagicBytes(void)    { return MagicBytes_;    }

   /////////////////////////////////////////////////////////////////////////////
   // These don't actually work while scanning in another thread!? 
   // The getLoadProgress* methods don't seem to update until after scan done
   uint64_t getTotalBlockchainBytes(void) const {return totalBlockchainBytes_;}
   uint16_t getTotalBlkFiles(void)        const {return numBlkFiles_;}
   uint64_t getLoadProgressBytes(void)    const {return bytesReadSoFar_;}
   uint32_t getLoadProgressBlocks(void)   const {return blocksReadSoFar_;}
   uint16_t getLoadProgressFiles(void)    const {return filesReadSoFar_;}

   /////////////////////////////////////////////////////////////////////////////
   void Reset(void);
   int32_t          getNumConfirmations(BinaryData txHash);
   BlockHeader &    getTopBlockHeader(void) ;
   BlockHeader &    getGenesisBlock(void) ;
   BlockHeader *    getHeaderByHeight(int index);
   BlockHeader *    getHeaderByHash(BinaryData const & blkHash);
   string           getBlockfilePath(void) {return blkFileDir_;}

   TxRef *          getTxRefPtrByHash(BinaryData const & txHash);
   Tx               getTxByHash(BinaryData const & txHash);

   //////////////////////////////////////////////////////////////////////////
   // Returns a pointer to the TxRef as it resides in the multimap node
   // There should only ever be exactly one copy
   TxRef *          insertTxRef(HashString const & txHash,
                                FileDataPtr & fdp,
                                BlockHeader * bhptr=NULL);

   uint32_t getTopBlockHeight(void) {return getTopBlockHeader().getBlockHeight();}

   bool isDirty(uint32_t numBlockToBeConsideredDirty=2016) const; 

   //uint32_t getNumTx(void) { return txHintMap_.size(); }
   uint32_t getNumHeaders(void) { return headerMap_.size(); }

   /////////////////////////////////////////////////////////////////////////////
   // If you register you wallet with the BDM, it will automatically maintain 
   // tx lists relevant to that wallet.  You can get away without registering
   // your wallet objects (using scanBlockchainForTx), but without the full 
   // blockchain in RAM, each scan will take 30-120 seconds.  Registering makes 
   // sure that the intial blockchain scan picks up wallet-relevant stuff as 
   // it goes, and does a full [re-]scan of the blockchain only if necessary.
   bool     registerWallet(BtcWallet* wallet, bool wltIsNew=false);
   void     unregisterWallet(BtcWallet* wlt) {registeredWallets_.erase(wlt);}
   bool     registerAddress(HashString addr160, bool isNew, uint32_t blk0);
   bool     registerNewAddress(HashString addr160);
   bool     registerImportedAddress(HashString addr160, uint32_t createBlk=0);
   bool     unregisterAddress(HashString addr160);
   uint32_t evalLowestBlockNextScan(void);
   uint32_t evalLowestAddressCreationBlock(void);
   bool     evalRescanIsRequired(void);
   uint32_t numBlocksToRescan(BtcWallet & wlt, uint32_t topBlk=UINT32_MAX);
   void     updateRegisteredAddresses(uint32_t newTopBlk);

   bool     walletIsRegistered(BtcWallet & wlt);
   bool     addressIsRegistered(HashString addr160);
   void     insertRegisteredTxIfNew(HashString txHash);
   void     registeredAddrScan( Tx & theTx );
   void     registeredAddrScan( uint8_t const * txptr,
                                uint32_t txSize=0,
                                vector<uint32_t> * txInOffsets=NULL,
                                vector<uint32_t> * txOutOffsets=NULL);
   void     resetRegisteredWallets(void);
   void     pprintRegisteredWallets(void);


   BtcWallet* createNewWallet(void);

   // Parsing requires the data TO ALREADY BE IN ITS PERMANENT MEMORY LOCATION
   // Pass in a wallet if you want to update the initialScanTxHashes_/OutPoints_
   bool     parseNewBlock(BinaryRefReader & rawBlockDataReader,
                          uint32_t fileIndex,
                          uint32_t thisHeaderOffset,
                          uint32_t blockSize);
                     


   // Does a full scan!
   uint32_t parseEntireBlockchain(uint32_t cacheSz=DEFAULT_CACHE_SIZE);

   // When we add new block data, we will need to store/copy it to its
   // permanent memory location before parsing it.
   // These methods return (blockAddSucceeded, newBlockIsTop, didCauseReorg)
   vector<bool>     addNewBlockData(   BinaryRefReader & brrRawBlock,
                                       uint32_t        fileIndex,
                                       uint32_t        thisHeaderOffset,
                                       uint32_t        blockSize);

   void             reassessAfterReorg(BlockHeader* oldTopPtr,
                                       BlockHeader* newTopPtr,
                                       BlockHeader* branchPtr );

   int  hasTxWithHash(BinaryData const & txhash);
   bool hasHeaderWithHash(BinaryData const & txhash) const;

   uint32_t getNumBlocks(void) const { return headerMap_.size(); }
   //uint32_t getNumTx(void) const { return txHintMap_.size(); }

   vector<BlockHeader*> getHeadersNotOnMainChain(void);

   vector<BlockHeader*>    prefixSearchHeaders(BinaryData const & searchStr);
   vector<TxRef*>          prefixSearchTx     (BinaryData const & searchStr);
   vector<BinaryData>      prefixSearchAddress(BinaryData const & searchStr);

   // Traverse the blockchain and update the wallet[s] with the relevant Tx data
   // See comments above the scanBlockchainForTx in the .cpp, for more info
   void scanBlockchainForTx(BtcWallet & myWallet,
                            uint32_t startBlknum=0,
                            uint32_t endBlknum=UINT32_MAX);

   void rescanBlocks(uint32_t blk0=0, uint32_t blk1=UINT32_MAX);

   // This will only be used by the above method, probably wouldn't be called
   // directly from any other code
   void scanRegisteredTxForWallet( BtcWallet & wlt,
                                   uint32_t blkStart=0,
                                   uint32_t blkEnd=UINT32_MAX);


 
   uint32_t       readBlkFileUpdate(void);
   bool           verifyBlkFileIntegrity(void);
   //vector<TxRef*> findAllNonStdTx(void);
   

   // For zero-confirmation tx-handling
   void enableZeroConf(string);
   void disableZeroConf(string);
   void readZeroConfFile(string);
   bool addNewZeroConfTx(BinaryData const & rawTx, uint64_t txtime, bool writeToFile);
   void purgeZeroConfPool(void);
   void pprintZeroConfPool(void);
   void rewriteZeroConfFile(void);
   void rescanWalletZeroConf(BtcWallet & wlt);
   bool isTxFinal(Tx & tx);


   // After reading in all headers, find the longest chain and set nextHash vals
   // TODO:  Figure out if there is an elegant way to deal with a forked 
   //        blockchain containing two equal-length chains
   bool organizeChain(bool forceRebuild=false);

   /////////////////////////////////////////////////////////////////////////////
   bool             isLastBlockReorg(void)     {return lastBlockWasReorg_;}
   set<HashString>  getTxJustInvalidated(void) {return txJustInvalidated_;}
   set<HashString>  getTxJustAffected(void)    {return txJustAffected_;}
   void             updateWalletAfterReorg(BtcWallet & wlt);
   void             updateWalletsAfterReorg(vector<BtcWallet*> wltvect);
   void             updateWalletsAfterReorg(set<BtcWallet*> wltset);

   // Use these two methods to get ALL information about your unused TxOuts
   //vector<UnspentTxOut> getUnspentTxOutsForWallet(BtcWallet & wlt, int sortType=-1);
   //vector<UnspentTxOut> getNonStdUnspentTxOutsForWallet(BtcWallet & wlt);

   ////////////////////////////////////////////////////////////////////////////////
   // We're going to need the BDM's help to get the sender for a TxIn since it
   // sometimes requires going and finding the TxOut from the distant past
   TxOut      getPrevTxOut(TxIn & txin);
   BinaryData getSenderAddr20(TxIn & txin);
   int64_t    getSentValue(TxIn & txin);


   /////////////////////////////////////////////////////////////////////////////
   // A couple random methods to expose internal data structures for testing.
   // These methods should not be used for nominal operation.
   //multimap<HashString, TxRef> &  getTxHintMapRef(void) { return txHintMap_; }
   map<HashString, BlockHeader> & getHeaderMapRef(void) { return headerMap_; }
   deque<BlockHeader*> &          getHeadersByHeightRef(void) { return headersByHeight_;}

private:

   /////////////////////////////////////////////////////////////////////////////
   // Start from a node, trace down to the highest solved block, accumulate
   // difficulties and difficultySum values.  Return the difficultySum of 
   // this block.
   double traceChainDown(BlockHeader & bhpStart);
   void   markOrphanChain(BlockHeader & bhpStart);
};


////////////////////////////////////////////////////////////////////////////////
//
// We have a problem with "classic" swig refusing to compile static functions,
// which gives me no way to access BDM which is a singleton class accessed by
// a static class method.  This class simply wraps the call to be invoked in
// python/swig
//
////////////////////////////////////////////////////////////////////////////////
class BlockDataManager
{
public:
   BlockDataManager(void) { bdm_ = &(BlockDataManager_LevelDB::GetInstance());}
   
   BlockDataManager_LevelDB & getBDM(void) { return *bdm_; }

private:
   BlockDataManager_LevelDB* bdm_;
};


#endif
