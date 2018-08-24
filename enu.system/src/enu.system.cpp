#include <enu.system/enu.system.hpp>
#include <enulib/dispatcher.hpp>

#include "producer_pay.cpp"
#include "delegate_bandwidth.cpp"
#include "voting.cpp"
#include "exchange_state.cpp"


namespace enumivosystem {

   system_contract::system_contract( account_name s )
   :native(s),
    _voters(_self,_self),
    _producers(_self,_self),
    _global(_self,_self),
    _global2(_self,_self),
    _rammarket(_self,_self)
   {
      //print( "construct system\n" );
      _gstate  = _global.exists() ? _global.get() : get_default_parameters();
      _gstate2 = _global2.exists() ? _global2.get() : enumivo_global_state2{};

      auto itr = _rammarket.find(S(4,RAMCORE));

      if( itr == _rammarket.end() ) {
         auto system_token_supply   = enumivo::token(N(enu.token)).get_supply(enumivo::symbol_type(system_token_symbol).name()).amount;
         if( system_token_supply > 0 ) {
            itr = _rammarket.emplace( _self, [&]( auto& m ) {
               m.supply.amount = 100000000000000ll;
               m.supply.symbol = S(4,RAMCORE);
               m.base.balance.amount = int64_t(_gstate.free_ram());
               m.base.balance.symbol = S(0,RAM);
               m.quote.balance.amount = system_token_supply / 1000;
               m.quote.balance.symbol = CORE_SYMBOL;
            });
         }
      } else {
         //print( "ram market already created" );
      }
   }

   enumivo_global_state system_contract::get_default_parameters() {
      enumivo_global_state dp;
      get_blockchain_parameters(dp);
      return dp;
   }

   block_timestamp system_contract::current_block_time() {
      const static block_timestamp cbt{ time_point{ microseconds{ static_cast<int64_t>( current_time() ) } } };
      return cbt;
   }

   system_contract::~system_contract() {
      _global.set( _gstate, _self );
      _global2.set( _gstate2, _self );
   }

   void system_contract::setram( uint64_t max_ram_size ) {
      require_auth( _self );

      enumivo_assert( _gstate.max_ram_size < max_ram_size, "ram may only be increased" ); /// decreasing ram might result market maker issues
      enumivo_assert( max_ram_size < 1024ll*1024*1024*1024*1024, "ram size is unrealistic" );
      enumivo_assert( max_ram_size > _gstate.total_ram_bytes_reserved, "attempt to set max below reserved" );

      auto delta = int64_t(max_ram_size) - int64_t(_gstate.max_ram_size);
      auto itr = _rammarket.find(S(4,RAMCORE));

      /**
       *  Increase the amount of ram for sale based upon the change in max ram size.
       */
      _rammarket.modify( itr, 0, [&]( auto& m ) {
         m.base.balance.amount += delta;
      });

      _gstate.max_ram_size = max_ram_size;
   }

   void system_contract::update_ram_supply() {
      auto cbt = current_block_time();

      if( cbt <= _gstate2.last_ram_increase ) return;

      auto itr = _rammarket.find(S(4,RAMCORE));
      auto new_ram = (cbt.slot - _gstate2.last_ram_increase.slot)*_gstate2.new_ram_per_block;
      _gstate.max_ram_size += new_ram;

      /**
       *  Increase the amount of ram for sale based upon the change in max ram size.
       */
      _rammarket.modify( itr, 0, [&]( auto& m ) {
         m.base.balance.amount += new_ram;
      });
      _gstate2.last_ram_increase = cbt;
   }

   /**
    *  Sets the rate of increase of RAM in bytes per block. It is capped by the uint16_t to
    *  a maximum rate of 3 TB per year.
    *
    *  If update_ram_supply hasn't been called for the most recent block, then new ram will
    *  be allocated at the old rate up to the present block before switching the rate.
    */
   void system_contract::setramrate( uint16_t bytes_per_block ) {
      require_auth( _self );

      update_ram_supply();
      _gstate2.new_ram_per_block = bytes_per_block;
   }

   void system_contract::setparams( const enumivo::blockchain_parameters& params ) {
      require_auth( N(enumivo) );
      (enumivo::blockchain_parameters&)(_gstate) = params;
      enumivo_assert( 3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

   void system_contract::setpriv( account_name account, uint8_t ispriv ) {
      require_auth( _self );
      set_privileged( account, ispriv );
   }
   
   void system_contract::setalimits( account_name account, int64_t ram, int64_t net, int64_t cpu ) {
      require_auth( N(enumivo) );
      user_resources_table userres( _self, account );
      auto ritr = userres.find( account );
      enumivo_assert( ritr == userres.end(), "only supports unlimited accounts" );
      set_resource_limits(account, ram, net, cpu);
   }

   void system_contract::rmvproducer( account_name producer ) {
      require_auth( _self );
      auto prod = _producers.find( producer );
      enumivo_assert( prod != _producers.end(), "producer not found" );
      _producers.modify( prod, 0, [&](auto& p) {
            p.deactivate();
         });
   }

   void system_contract::bidname( account_name bidder, account_name newname, asset bid ) {
      require_auth( bidder );
      enumivo_assert( enumivo::name_suffix(newname) == newname, "you can only bid on top-level suffix" );

      enumivo_assert( newname != 0, "the empty name is not a valid account name to bid on" );
      enumivo_assert( (newname & 0xFull) == 0, "13 character names are not valid account names to bid on" );
      enumivo_assert( (newname & 0x1F0ull) == 0, "accounts with 12 character names and no dots can be created without bidding required" );
      enumivo_assert( !is_account( newname ), "account already exists" );
      enumivo_assert( bid.symbol == asset().symbol, "asset must be system token" );
      enumivo_assert( bid.amount > 0, "insufficient bid" );

      INLINE_ACTION_SENDER(enumivo::token, transfer)( N(enu.token), {bidder,N(active)},
                                                    { bidder, N(enu.names), bid, std::string("bid name ")+(name{newname}).to_string()  } );

      name_bid_table bids(_self,_self);
      print( name{bidder}, " bid ", bid, " on ", name{newname}, "\n" );
      auto current = bids.find( newname );
      if( current == bids.end() ) {
         bids.emplace( bidder, [&]( auto& b ) {
            b.newname = newname;
            b.high_bidder = bidder;
            b.high_bid = bid.amount;
            b.last_bid_time = current_time();
         });
      } else {
         enumivo_assert( current->high_bid > 0, "this auction has already closed" );
         enumivo_assert( bid.amount - current->high_bid > (current->high_bid / 10), "must increase bid by 10%" );
         enumivo_assert( current->high_bidder != bidder, "account is already highest bidder" );

         bid_refund_table refunds_table(_self, newname);

         auto it = refunds_table.find( current->high_bidder );
         if ( it != refunds_table.end() ) {
            refunds_table.modify( it, 0, [&](auto& r) {
                  r.amount += asset( current->high_bid, system_token_symbol );
               });
         } else {
            refunds_table.emplace( bidder, [&](auto& r) {
                  r.bidder = current->high_bidder;
                  r.amount = asset( current->high_bid, system_token_symbol );
               });
         }

         action a( {N(enumivo),N(active)}, N(enumivo), N(bidrefund), std::make_tuple( current->high_bidder, newname ) );
         transaction t;
         t.actions.push_back( std::move(a) );
         t.delay_sec = 0;
         uint128_t deferred_id = (uint128_t(newname) << 64) | current->high_bidder;
         cancel_deferred( deferred_id );
         t.send( deferred_id, bidder );

         bids.modify( current, bidder, [&]( auto& b ) {
            b.high_bidder = bidder;
            b.high_bid = bid.amount;
            b.last_bid_time = current_time();
         });
      }
   }

   void system_contract::bidrefund( account_name bidder, account_name newname ) {
      bid_refund_table refunds_table(_self, newname);
      auto it = refunds_table.find( bidder );
      enumivo_assert( it != refunds_table.end(), "refund not found" );
      INLINE_ACTION_SENDER(enumivo::token, transfer)( N(enu.token), {{N(enu.names),N(active)},{bidder,N(active)}},
                                                    { N(enu.names), bidder, asset(it->amount),
                                                       std::string("refund bid on name ")+(name{newname}).to_string()  } );
      refunds_table.erase( it );
   }

   /**
    *  Called after a new account is created. This code enforces resource-limits rules
    *  for new accounts as well as new account naming conventions.
    *
    *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
    *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
    *  who can create accounts with the creator's name as a suffix.
    *
    */
   void native::newaccount( account_name     creator,
                            account_name     newact
                            /*  no need to parse authorites
                            const authority& owner,
                            const authority& active*/ ) {

      if( creator != _self ) {

         //enumivo_assert( newact != N(enumivo.prods), "will cause collision" ); //with enumivo.prods
         //line above not needed since no collision will take place according to @iamveritas
         //but better safe than sorry

         auto tmp = newact >> 4;
         bool has_dot = false;

         for( uint32_t i = 0; i < 12; ++i ) {
           has_dot |= !(tmp & 0x1f);
           tmp >>= 5;
         }
         if( has_dot ) { // or is less than 12 characters
            auto suffix = enumivo::name_suffix(newact);
            if( suffix == newact ) {
               name_bid_table bids(_self,_self);
               auto current = bids.find( newact );
               enumivo_assert( current != bids.end(), "no active bid for name" );
               enumivo_assert( current->high_bidder == creator, "only highest bidder can claim" );
               enumivo_assert( current->high_bid < 0, "auction for name is not closed yet" );
               bids.erase( current );
            } else {
               enumivo_assert( creator == suffix, "only suffix may create this account" );
            }
         }
      }

      user_resources_table  userres( _self, newact);

      userres.emplace( newact, [&]( auto& res ) {
        res.owner = newact;
      });

      set_resource_limits( newact, 0, 0, 0 );
   }

} /// enu.system


ENUMIVO_ABI( enumivosystem::system_contract,
     // native.hpp (newaccount definition is actually in enu.system.cpp)
     (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)
     // enu.system.cpp
     (setram)(setramrate)(setparams)(setpriv)(setalimits)(rmvproducer)(bidname)(bidrefund)
     // delegate_bandwidth.cpp
     (buyrambytes)(buyram)(sellram)(delegatebw)(undelegatebw)(refund)
     // voting.cpp
     (regproducer)(unregprod)(voteproducer)(regproxy)
     // producer_pay.cpp
     (onblock)(claimrewards)
)
