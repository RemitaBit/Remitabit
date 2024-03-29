#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/market_operations.hpp>
#include <bts/blockchain/slate_operations.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/blockchain/transaction.hpp>

#include <fc/io/raw_variant.hpp>

namespace bts { namespace blockchain {

   digest_type transaction::digest( const digest_type& chain_id )const
   {
      fc::sha256::encoder enc;
      fc::raw::pack(enc,*this);
      fc::raw::pack(enc,chain_id);
      return enc.result();
   }

   transaction_id_type signed_transaction::id()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack( enc, *this );
      return fc::ripemd160::hash( enc.result() );
   }

   size_t signed_transaction::data_size()const
   {
      fc::datastream<size_t> ds;
      fc::raw::pack(ds,*this);
      return ds.tellp();
   }

   void signed_transaction::sign( const fc::ecc::private_key& signer, const digest_type& chain_id )
   {
      signatures.push_back( signer.sign_compact( digest( chain_id ) ) );
   }

   public_key_type signed_transaction::get_signing_key( const size_t sig_index, const digest_type& chain_id )const
   { try {
       return fc::ecc::public_key( signatures.at( sig_index ), this->digest( chain_id ), false );
   } FC_CAPTURE_AND_RETHROW( (sig_index)(chain_id) ) }

   void transaction::define_slate( const set<account_id_type>& slate )
   { try {
       define_slate_operation op;
       for( const account_id_type id : slate ) op.slate.push_back( id );
       // Insert at head because subsequent evaluation may depend on the pending slate record
       operations.insert( operations.begin(), std::move( op ) );
   } FC_CAPTURE_AND_RETHROW( (slate) ) }

   void transaction::burn( const asset& quantity, account_id_type for_or_against, const string& message, const optional<signature_type>& sig )
   {
      if( message.size() ) FC_ASSERT( quantity.asset_id == 0 );
      operations.emplace_back( burn_operation( quantity, for_or_against, message, sig) );
   }

   void transaction::bid( const asset& quantity,
                          const price& price_per_unit,
                          const address& owner )
   {
      bid_operation op;
      op.amount = quantity.amount;
      op.bid_index.order_price = price_per_unit;
      op.bid_index.owner = owner;

      operations.emplace_back( std::move( op ) );
   }

   void transaction::ask( const asset& quantity,
                          const price& price_per_unit,
                          const address& owner )
   {
      ask_operation op;
      op.amount = quantity.amount;
      op.ask_index.order_price = price_per_unit;
      op.ask_index.owner = owner;

      operations.emplace_back( std::move( op ) );
   }
   void transaction::relative_bid( const asset& quantity,
                          const price& delta_price_per_unit,
                          const optional<price>& limit,
                          const address& owner )
   {
      relative_bid_operation op;
      op.amount = quantity.amount;
      op.bid_index.order_price = delta_price_per_unit;
      op.bid_index.owner = owner;
      op.limit_price = limit;

      operations.emplace_back( std::move( op ) );
   }

   void transaction::relative_ask( const asset& quantity,
                          const price& delta_price_per_unit,
                          const optional<price>& limit,
                          const address& owner )
   {
      relative_ask_operation op;
      op.amount = quantity.amount;
      op.ask_index.order_price = delta_price_per_unit;
      op.ask_index.owner = owner;
      op.limit_price = limit;

      operations.emplace_back( std::move( op ) );
   }

   void transaction::short_sell( const asset& quantity,
                                 const price& interest_rate,
                                 const address& owner,
                                 const optional<price>& limit_price )
   {
      short_operation op;
      op.amount = quantity.amount;
      op.short_index.order_price = interest_rate;
      op.short_index.owner = owner;
      op.short_index.limit_price = limit_price;

      operations.emplace_back( std::move( op ) );
   }

   void transaction::withdraw( const balance_id_type& account,
                               share_type             amount )
   { try {
      FC_ASSERT( amount > 0, "amount: ${amount}", ("amount",amount) );
      operations.emplace_back( withdraw_operation( account, amount ) );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account",account)("amount",amount) ) }

   void transaction::withdraw_pay( const account_id_type account,
                                   share_type             amount )
   {
      FC_ASSERT( amount > 0, "amount: ${amount}", ("amount",amount) );
      operations.emplace_back( withdraw_pay_operation( amount, account ) );
   }

   void transaction::deposit( const address& owner, const asset& amount )
   {
      FC_ASSERT( amount.amount > 0, "amount: ${amount}", ("amount",amount) );
      operations.emplace_back( deposit_operation( owner, amount ) );
   }

   void transaction::deposit_multisig( const multisig_meta_info& multsig_info, const asset& amount )
   {
      FC_ASSERT( amount.amount > 0, "amount: ${amount}", ("amount",amount) );
      deposit_operation op;
      op.amount = amount.amount;
      op.condition = withdraw_condition( withdraw_with_multisig{multsig_info.required,multsig_info.owners}, amount.asset_id );
      operations.emplace_back( std::move( op ) );
   }


   public_key_type transaction::deposit_to_account( fc::ecc::public_key receiver_key,
                                                    asset amount,
                                                    fc::ecc::private_key from_key,
                                                    const std::string& memo_message,
                                                    const fc::ecc::public_key& memo_pub_key,
                                                    fc::ecc::private_key one_time_private_key,
                                                    memo_flags_enum memo_type,
                                                    bool use_stealth_address )
   {
      withdraw_with_signature by_account;
      auto receiver_address_key = by_account.encrypt_memo_data( one_time_private_key,
                                                                receiver_key,
                                                                from_key,
                                                                memo_message,
                                                                memo_pub_key,
                                                                memo_type,
                                                                use_stealth_address );

      deposit_operation op;
      op.amount = amount.amount;
      op.condition = withdraw_condition( by_account, amount.asset_id );
      operations.emplace_back( std::move( op ) );
      return receiver_address_key;
   }
   void transaction::release_escrow( const address& escrow_account,
                                     const address& released_by,
                                     share_type amount_to_sender,
                                     share_type amount_to_receiver )
   {
       release_escrow_operation op;
       op.escrow_id          = escrow_account;
       op.released_by        = released_by;
       op.amount_to_receiver = amount_to_receiver;
       op.amount_to_sender   = amount_to_sender;
       operations.emplace_back( std::move( op ) );
   }

   public_key_type transaction::deposit_to_escrow(
                                        fc::ecc::public_key receiver_key,
                                        fc::ecc::public_key escrow_key,
                                        digest_type agreement,
                                        asset amount,
                                        fc::ecc::private_key from_key,
                                        const std::string& memo_message,
                                        const fc::ecc::public_key& memo_pub_key,
                                        fc::ecc::private_key one_time_private_key,
                                        memo_flags_enum memo_type
                                      )
   {
      withdraw_with_escrow by_escrow;
      auto receiver_pub_key = by_escrow.encrypt_memo_data( one_time_private_key,
                                 receiver_key,
                                 from_key,
                                 memo_message,
                                 memo_pub_key,
                                 memo_type );
      by_escrow.escrow = escrow_key;
      by_escrow.agreement_digest = agreement;

      deposit_operation op;
      op.amount = amount.amount;
      op.condition = withdraw_condition( by_escrow, amount.asset_id );

      operations.emplace_back( std::move( op ) );
      return receiver_pub_key;
   }


   void transaction::register_account( const std::string& name,
                                       const fc::variant& public_data,
                                       const public_key_type& master,
                                       const public_key_type& active,
                                       uint8_t pay_rate,
                                       optional<account_meta_info> info )
   {
      register_account_operation op( name, public_data, master, active, pay_rate );
      op.meta_data = info;
      operations.emplace_back( std::move( op ) );
   }

   void transaction::update_account( account_id_type account_id,
                                  uint8_t delegate_pay_rate,
                                  const fc::optional<fc::variant>& public_data,
                                  const fc::optional<public_key_type>& active   )
   {
      update_account_operation op;
      op.account_id = account_id;
      op.public_data = public_data;
      op.active_key = active;
      op.delegate_pay_rate = delegate_pay_rate;
      operations.emplace_back( std::move( op ) );
   }

   void transaction::create_asset( const std::string& symbol,
                                   const std::string& name,
                                   const std::string& description,
                                   const fc::variant& data,
                                   account_id_type issuer_id,
                                   share_type max_share_supply,
                                   uint64_t precision )
   {
      FC_ASSERT( max_share_supply > 0 );
      FC_ASSERT( max_share_supply <= BTS_BLOCKCHAIN_MAX_SHARES );
      create_asset_operation op;
      op.symbol = symbol;
      op.name = name;
      op.description = description;
      op.public_data = data;
      op.issuer_account_id = issuer_id;
      op.maximum_share_supply = max_share_supply;
      op.precision = precision;
      operations.emplace_back( std::move( op ) );
   }

   void transaction::update_asset( const asset_id_type asset_id,
                                   const optional<string>& name,
                                   const optional<string>& description,
                                   const optional<variant>& public_data,
                                   const optional<double>& maximum_share_supply,
                                   const optional<uint64_t>& precision )
   {
       operations.emplace_back( update_asset_operation{ asset_id, name, description, public_data, maximum_share_supply, precision } );
   }
   void transaction::update_asset_ext( const asset_id_type asset_id,
                                   const optional<string>& name,
                                   const optional<string>& description,
                                   const optional<variant>& public_data,
                                   const optional<double>& maximum_share_supply,
                                   const optional<uint64_t>& precision,
                                   const share_type issuer_fee,
                                   uint16_t market_fee,
                                   uint32_t  flags,
                                   uint32_t issuer_permissions,
                                   account_id_type issuer_account_id,
                                   uint32_t required_sigs,
                                   const vector<address>& authority
                                   )
   {
       multisig_meta_info auth_info;
       auth_info.required = required_sigs;
       auth_info.owners.insert( authority.begin(), authority.end() );
       update_asset_ext_operation op( update_asset_operation{asset_id, name, description, public_data, maximum_share_supply, precision} );
       op.flags = flags;
       op.issuer_permissions = issuer_permissions;
       op.issuer_account_id = issuer_account_id;
       op.transaction_fee = issuer_fee,
       op.market_fee = market_fee;
       op.authority = auth_info;
       operations.emplace_back( std::move( op ) );
   }

   void transaction::issue( const asset& amount_to_issue )
   {
      operations.emplace_back( issue_asset_operation( amount_to_issue ) );
   }

   void transaction::cover( const asset& cover_amount,
                            const market_index_key& order_idx )
   {
      operations.emplace_back( cover_operation( cover_amount.amount, order_idx ) );
   }

   void transaction::add_collateral( share_type collateral_amount, const market_index_key& order_idx )
   {
      operations.emplace_back( add_collateral_operation( collateral_amount, order_idx ) );
   }

   void transaction::publish_feed( asset_id_type feed_id, account_id_type delegate_id, fc::variant value )
   {
      operations.emplace_back( update_feed_operation{ feed_index{ feed_id, delegate_id }, value } );
   }

   void transaction::update_signing_key( const account_id_type account_id, const public_key_type& signing_key )
   {
       operations.emplace_back( update_signing_key_operation{ account_id, signing_key } );
   }

   void transaction::update_balance_vote( const balance_id_type& balance_id, const optional<address>& new_restricted_owner )
   {
       operations.emplace_back( update_balance_vote_operation{ balance_id, new_restricted_owner } );
   }

   void transaction::set_slates( const slate_id_type slate_id )
   {
       for( size_t i = 0; i < operations.size(); ++i )
       {
           const operation& op = operations.at( i );
           switch( operation_type_enum( op.type ) )
           {
               case deposit_op_type:
               {
                   deposit_operation deposit_op = op.as<deposit_operation>();
                   if( deposit_op.condition.asset_id == 0 )
                   {
                       deposit_op.condition.slate_id = slate_id;
                       operations[ i ] = deposit_op;
                   }
                   break;
               }
               case update_balance_vote_op_type:
               {
                   update_balance_vote_operation update_balance_vote_op = op.as<update_balance_vote_operation>();
                   update_balance_vote_op.new_slate = slate_id;
                   operations[ i ] = update_balance_vote_op;
                   break;
               }
               default:
               {
                   break;
               }
           }
       }
   }

   bool transaction::is_cancel()const
   {
      for( const auto& op : operations )
      {
          switch( operation_type_enum( op.type ) )
          {
              case bid_op_type:
                  if( op.as<bid_operation>().amount < 0 ) return true;
                  break;
              case ask_op_type:
                  if( op.as<ask_operation>().amount < 0 ) return true;
                  break;
              case short_op_type:
                  if( op.as<short_operation_v1>().amount < 0 ) return true;
                  break;
              case short_op_v2_type:
                  if( op.as<short_operation>().amount < 0 ) return true;
                  break;
              default:
                  break;
          }
      }
      return false;
   }

    void transaction::authorize_key( asset_id_type asset_id, const address& owner, object_id_type meta )
    {
       authorize_operation op;
       op.asset_id = asset_id;
       op.owner = owner;
       op.meta_id = meta;
       operations.emplace_back( std::move( op ) );
    }

} } // bts::blockchain
