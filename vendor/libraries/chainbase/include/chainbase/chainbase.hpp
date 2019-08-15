#pragma once

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/set.hpp>
#include <boost/interprocess/containers/flat_map.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

#include <boost/multi_index_container.hpp>

#include <boost/chrono.hpp>
#include <boost/config.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/throw_exception.hpp>
#include <boost/tuple/tuple_io.hpp>

#include <array>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>

#ifndef CHAINBASE_NUM_RW_LOCKS
   #define CHAINBASE_NUM_RW_LOCKS 10
#endif

#ifdef CHAINBASE_CHECK_LOCKING
   #define CHAINBASE_REQUIRE_READ_LOCK(m, t) require_read_lock(m, typeid(t).name())
   #define CHAINBASE_REQUIRE_WRITE_LOCK(m, t) require_write_lock(m, typeid(t).name())
#else
   #define CHAINBASE_REQUIRE_READ_LOCK(m, t)
   #define CHAINBASE_REQUIRE_WRITE_LOCK(m, t)
#endif

namespace chainbase {

   namespace bip = boost::interprocess;
   namespace bfs = boost::filesystem;
   using std::unique_ptr;
   using std::vector;

   template<typename T>
   using allocator = bip::allocator<T, bip::managed_mapped_file::segment_manager>;

   typedef bip::basic_string< char, std::char_traits< char >, allocator< char > > shared_string;

   template<typename T>
   using shared_vector = std::vector<T, allocator<T> >;

   template<typename T>
   using shared_set = std::set<T, std::less<T>, allocator<T>>;

   constexpr char _db_dirty_flag_string[] = "db_dirty_flag";

   struct strcmp_less
   {
      bool operator()( const shared_string& a, const shared_string& b )const
      {
         return less( a.c_str(), b.c_str() );
      }

      bool operator()( const shared_string& a, const std::string& b )const
      {
         return less( a.c_str(), b.c_str() );
      }

      bool operator()( const std::string& a, const shared_string& b )const
      {
         return less( a.c_str(), b.c_str() );
      }
      private:
         inline bool less( const char* a, const char* b )const
         {
            return std::strcmp( a, b ) < 0;
         }
   };

   typedef boost::interprocess::interprocess_sharable_mutex read_write_mutex;
   typedef boost::interprocess::sharable_lock< read_write_mutex > read_lock;
   typedef boost::unique_lock< read_write_mutex > write_lock;

   /**
    *  Object ID type that includes the type of the object it references
    */
   template<typename T>
   class oid {
      public:
         oid( int64_t i = 0 ):_id(i){}

         oid& operator++() { ++_id; return *this; }

         friend bool operator < ( const oid& a, const oid& b ) { return a._id < b._id; }
         friend bool operator > ( const oid& a, const oid& b ) { return a._id > b._id; }
         friend bool operator == ( const oid& a, const oid& b ) { return a._id == b._id; }
         friend bool operator != ( const oid& a, const oid& b ) { return a._id != b._id; }
         friend std::ostream& operator<<(std::ostream& s, const oid& id) {
            s << boost::core::demangle(typeid(oid<T>).name()) << '(' << id._id << ')'; return s;
         }

         int64_t _id = 0;
   };

   template<uint16_t TypeNumber, typename Derived>
   struct object
   {
      typedef oid<Derived> id_type;
      static const uint16_t type_id = TypeNumber;
   };

   /** this class is ment to be specified to enable lookup of index type by object type using
    * the SET_INDEX_TYPE macro.
    **/
   template<typename T>
   struct get_index_type {};

   /**
    *  This macro must be used at global scope and OBJECT_TYPE and INDEX_TYPE must be fully qualified
    */
   #define CHAINBASE_SET_INDEX_TYPE( OBJECT_TYPE, INDEX_TYPE )  \
   namespace chainbase { template<> struct get_index_type<OBJECT_TYPE> { typedef INDEX_TYPE type; }; }

   #define CHAINBASE_DEFAULT_CONSTRUCTOR( OBJECT_TYPE ) \
   template<typename Constructor, typename Allocator> \
   OBJECT_TYPE( Constructor&& c, Allocator&&  ) { c(*this); }

   template< typename value_type >
   class undo_state
   {
      public:
         typedef typename value_type::id_type                      id_type;
         typedef allocator< std::pair<const id_type, value_type> > id_value_allocator_type;
         typedef allocator< id_type >                              id_allocator_type;

         template<typename T>
         undo_state( allocator<T> al )
         :old_values( id_value_allocator_type( al.get_segment_manager() ) ),
          removed_values( id_value_allocator_type( al.get_segment_manager() ) ),
          new_ids( id_allocator_type( al.get_segment_manager() ) ){}

         typedef boost::interprocess::map< id_type, value_type, std::less<id_type>, id_value_allocator_type >  id_value_type_map;
         typedef boost::interprocess::set< id_type, std::less<id_type>, id_allocator_type >                    id_type_set;

         id_value_type_map            old_values;
         id_value_type_map            removed_values;
         id_type_set                  new_ids;
         id_type                      old_next_id = 0;
         int64_t                      revision = 0;
   };

   template< typename value_type >
   class cache_state
   {
      public:
         typedef typename value_type::id_type                      id_type;
         typedef allocator< std::pair<const id_type, value_type> > id_value_allocator_type;
         typedef allocator< id_type >                              id_allocator_type;

         template<typename T>
         cache_state( allocator<T> al )
         :new_values( id_value_allocator_type( al.get_segment_manager() ) ),
          modify_values( id_value_allocator_type( al.get_segment_manager() ) ),
          removed_ids( id_allocator_type( al.get_segment_manager() ) ){}

         typedef boost::interprocess::map< id_type, value_type, std::less<id_type>, id_value_allocator_type >  id_value_type_map;
         typedef boost::interprocess::set< id_type, std::less<id_type>, id_allocator_type >                    id_type_set;

         id_value_type_map            new_values;
         id_value_type_map            modify_values;
         id_type_set                  removed_ids;
   };
   /**
    * The code we want to implement is this:
    *
    * ++target; try { ... } finally { --target }
    *
    * In C++ the only way to implement finally is to create a class
    * with a destructor, so that's what we do here.
    */
   class int_incrementer
   {
      public:
         int_incrementer( int32_t& target ) : _target(target)
         { ++_target; }
         ~int_incrementer()
         { --_target; }

         int32_t get()const
         { return _target; }

      private:
         int32_t& _target;
   };

   /**
    *  The value_type stored in the multiindex container must have a integer field with the name 'id'.  This will
    *  be the primary key and it will be assigned and managed by generic_index.
    *
    *  Additionally, the constructor for value_type must take an allocator
    */
   template<typename MultiIndexType>
   class generic_index
   {
      public:
         typedef bip::managed_mapped_file::segment_manager             segment_manager_type;
         typedef MultiIndexType                                        index_type;
         typedef typename index_type::value_type                       value_type;
         typedef bip::allocator< generic_index, segment_manager_type > allocator_type;
         typedef undo_state< value_type >                              undo_state_type;
         typedef cache_state< value_type >                              cache_state_type;

         generic_index( allocator<value_type> a, bool cache_on=true )
         :_stack(a),_cache(a),_cache_on(cache_on),_indices( a ),_backup_indices( a ),_size_of_value_type( sizeof(typename MultiIndexType::node_type) ),_size_of_this(sizeof(*this)){}

         void validate()const {
            if( sizeof(typename MultiIndexType::node_type) != _size_of_value_type || sizeof(*this) != _size_of_this )
               BOOST_THROW_EXCEPTION( std::runtime_error("content of memory does not match data expected by executable") );
         }

         /**
          * Construct a new element in the multi_index_container for backup.
          */
         template<typename Constructor>
         const value_type& backup_emplace( Constructor&& c ) {
            auto constructor = [&]( value_type& v ) {
               c( v );
            };

            auto insert_result = _backup_indices.emplace( constructor, _backup_indices.get_allocator() );
            if( !insert_result.second ) {
               BOOST_THROW_EXCEPTION( std::logic_error("could not insert backup object, most likely a uniqueness constraint was violated") );
            }
            return *insert_result.first;
         }

         template<typename Constructor>
         const value_type& emplace( Constructor&& c ) {
            auto new_id = _next_id;

            auto constructor = [&]( value_type& v ) {
               v.id = new_id;
               c( v );
            };

            auto insert_result = _indices.emplace( constructor, _indices.get_allocator() );

            if( !insert_result.second ) {
               BOOST_THROW_EXCEPTION( std::logic_error("could not insert object, most likely a uniqueness constraint was violated") );
            }

            ++_next_id;
            cache_create(*insert_result.first);
            on_create( *insert_result.first );
            return *insert_result.first;
         }

         template<typename Modifier>
         void modify( const value_type& obj, Modifier&& m ) {
            on_modify( obj );
            auto ok = _indices.modify( _indices.iterator_to( obj ), m );
            if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "modify: Could not modify object, most likely a uniqueness constraint was violated" ) );
            cache_modify( obj );
         }

         void remove( const value_type& obj ) {
            on_remove( obj );
            cache_remove( obj );
            _indices.erase( _indices.iterator_to( obj ) );
         }

         template<typename CompatibleKey>
         const value_type* find( CompatibleKey&& key )const {
            auto itr = _indices.find( std::forward<CompatibleKey>(key) );
            if( itr != _indices.end() ) return &*itr;
            return nullptr;
         }

         template<typename CompatibleKey>
         const value_type& get( CompatibleKey&& key )const {
            auto ptr = find( key );
            if( !ptr ) {
               std::stringstream ss;
               ss << "key not found (" << boost::core::demangle( typeid( key ).name() ) << "): " << key;
               BOOST_THROW_EXCEPTION( std::out_of_range( ss.str().c_str() ) );
            }
            return *ptr;
         }

         const index_type& indices()const { return _indices; }
         const index_type& backup_indices() const { return _backup_indices;}
         // void backup_clear() { return _backup_indices.clear();}

         class session {
            public:
               session( session&& mv )
               :_index(mv._index),_apply(mv._apply){ mv._apply = false; }

               ~session() {
                  if( _apply ) {
                     _index.undo();
                  }
               }

               /** leaves the UNDO state on the stack when session goes out of scope */
               void push( bool ws = false )   {
                  _apply = false;
                  if ( _index._ws )
                  { _index._ws = false; return; }

                  _index.squash_cache();

                  /*ws == True, we must make sure that _cache size is 1, _cache will be used to create latest ws file*/
                  while( ws && _index._cache.size() >= 2 )
                     _index.squash_cache();

                  if ( ws )
                     _index._ws = true;
               }
               /** combines this session with the prior session */
               void squash() { if( _apply ) _index.squash(); _apply = false; }
               void undo()   { if( _apply ) _index.undo();  _apply = false; }

               session& operator = ( session&& mv ) {
                  if( this == &mv ) return *this;
                  if( _apply ) _index.undo();
                  _apply = mv._apply;
                  mv._apply = false;
                  return *this;
               }

               int64_t revision()const { return _revision; }

            private:
               friend class generic_index;

               session( generic_index& idx, int64_t revision )
               :_index(idx),_revision(revision) {
                  if( revision == -1 )
                     _apply = false;
               }

               generic_index& _index;
               bool           _apply = true;
               int64_t        _revision = 0;
         };

         session start_undo_session( bool enabled ) {
            if( enabled ) {
               _stack.emplace_back( _indices.get_allocator() );
               _stack.back().old_next_id = _next_id;
               _stack.back().revision = ++_revision;

               if( _cache_on){
                  _cache.emplace_back( _indices.get_allocator() );
               }
               return session( *this, _revision );
            } else {
               return session( *this, -1 );
            }
         }

         int64_t revision()const { return _revision; }

         /**
          *  Restores the state to how it was prior to the current session discarding all changes
          *  made between the last revision and the current revision.
          */
         void undo() {
            if( !enabled() ) return;

            if( _cache.size() ) _cache.pop_back();

            const auto& head = _stack.back();

            /*The order of operation must be create, modify, remove.That is because it maybe conflict
            between diff operations of unique key.
            Ex: class A have a int unique key t
               t1 = 1;
               Oper A:
                  modify: t1 --> 3;
               Oper B:
                  create: t2 = 1;

               In this case, undo() must be undo  Oper B , then undo oper A. Otherwise, it will faild when modify t1
               from 3 to 1;
            */
            for( auto id : head.new_ids )
            {
               _indices.erase( _indices.find( id ) );
            }
            _next_id = head.old_next_id;

            for( auto& item : head.old_values ) {
               auto ok = _indices.modify( _indices.find( item.second.id ), [&]( value_type& v ) {
                  v = item.second;
               });
               if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "undo: Could not modify object, most likely a uniqueness constraint was violated" ) );
            }

            for( auto& item : head.removed_values ) {
               bool ok = _indices.emplace( item.second ).second;
               if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "undo: Could not restore object, most likely a uniqueness constraint was violated" ) );
            }

            _stack.pop_back();
            --_revision;
         }

         /**
          *  This method works similar to git squash, it merges the change set from the two most
          *  recent into one 
          *
          *  This method does not change the state of the index, only the state of the cache buffer.
          */
         void squash_cache(){
             if( !_cache_on|| _cache.size()<2  ) {return;};
             auto& cache = _cache.back();
             auto& prev_cache = *(_cache.end()-2);

             for( const auto& item : cache.modify_values )
             {
                 auto it = prev_cache.new_values.find( item.second.id );
                 if( it != prev_cache.new_values.end() )
                 {
                     it->second= item.second;
                     continue;
                 }
                 it = prev_cache.modify_values.find( item.second.id );
                 if( it != prev_cache.modify_values.end() )
                 {
                     it->second= item.second;
                     continue;
                 }
                 assert( prev_cache.removed_ids.find(item.second.id) == prev_cache.removed_ids.end() );
                 prev_cache.modify_values.emplace( std::move(item) );
             }

             for( auto& item : cache.new_values )
                 prev_cache.new_values.emplace( std::move(item));

             for( auto id: cache.removed_ids )
             {
                 auto it = prev_cache.new_values.find(id);
                 if(  it != prev_cache.new_values.end() )
                 {
                     prev_cache.new_values.erase(id);
                     continue;
                 }
                 it = prev_cache.modify_values.find(id);
                 if( it != prev_cache.modify_values.end() )
                 {
                     prev_cache.removed_ids.emplace( id );
                     prev_cache.modify_values.erase(id);
                     continue;
                 }
                 assert( prev_cache.removed_ids.find( id ) == prev_cache.removed_ids.end() );
                 prev_cache.removed_ids.emplace( id );
             }
             _cache.pop_back();
         }

         void squash()
         {
            if( !enabled() ) return;
            if( _stack.size() == 1 ) {
               _stack.pop_front();
               --_revision;
               return;
            }

            auto& state = _stack.back();
            auto& prev_state = _stack[_stack.size()-2];

            // An object's relationship to a state can be:
            // in new_ids            : new
            // in old_values (was=X) : upd(was=X)
            // in removed (was=X)    : del(was=X)
            // not in any of above   : nop
            //
            // When merging A=prev_state and B=state we have a 4x4 matrix of all possibilities:
            //
            //                   |--------------------- B ----------------------|
            //
            //                +------------+------------+------------+------------+
            //                | new        | upd(was=Y) | del(was=Y) | nop        |
            //   +------------+------------+------------+------------+------------+
            // / | new        | N/A        | new       A| nop       C| new       A|
            // | +------------+------------+------------+------------+------------+
            // | | upd(was=X) | N/A        | upd(was=X)A| del(was=X)C| upd(was=X)A|
            // A +------------+------------+------------+------------+------------+
            // | | del(was=X) | N/A        | N/A        | N/A        | del(was=X)A|
            // | +------------+------------+------------+------------+------------+
            // \ | nop        | new       B| upd(was=Y)B| del(was=Y)B| nop      AB|
            //   +------------+------------+------------+------------+------------+
            //
            // Each entry was composed by labelling what should occur in the given case.
            //
            // Type A means the composition of states contains the same entry as the first of the two merged states for that object.
            // Type B means the composition of states contains the same entry as the second of the two merged states for that object.
            // Type C means the composition of states contains an entry different from either of the merged states for that object.
            // Type N/A means the composition of states violates causal timing.
            // Type AB means both type A and type B simultaneously.
            //
            // The merge() operation is defined as modifying prev_state in-place to be the state object which represents the composition of
            // state A and B.
            //
            // Type A (and AB) can be implemented as a no-op; prev_state already contains the correct value for the merged state.
            // Type B (and AB) can be implemented by copying from state to prev_state.
            // Type C needs special case-by-case logic.
            // Type N/A can be ignored or assert(false) as it can only occur if prev_state and state have illegal values
            // (a serious logic error which should never happen).
            //

            // We can only be outside type A/AB (the nop path) if B is not nop, so it suffices to iterate through B's three containers.

            for( const auto& item : state.old_values )
            {
               if( prev_state.new_ids.find( item.second.id ) != prev_state.new_ids.end() )
               {
                  // new+upd -> new, type A
                  continue;
               }
               if( prev_state.old_values.find( item.second.id ) != prev_state.old_values.end() )
               {
                  // upd(was=X) + upd(was=Y) -> upd(was=X), type A
                  continue;
               }
               // del+upd -> N/A
               assert( prev_state.removed_values.find(item.second.id) == prev_state.removed_values.end() );
               // nop+upd(was=Y) -> upd(was=Y), type B
               prev_state.old_values.emplace( std::move(item) );
            }

            // *+new, but we assume the N/A cases don't happen, leaving type B nop+new -> new
            for( auto id : state.new_ids )
               prev_state.new_ids.insert(id);

            // *+del
            for( auto& obj : state.removed_values )
            {
               if( prev_state.new_ids.find(obj.second.id) != prev_state.new_ids.end() )
               {
                  // new + del -> nop (type C)
                  prev_state.new_ids.erase(obj.second.id);
                  continue;
               }
               auto it = prev_state.old_values.find(obj.second.id);
               if( it != prev_state.old_values.end() )
               {
                  // upd(was=X) + del(was=Y) -> del(was=X)
                  prev_state.removed_values.emplace( std::move(*it) );
                  prev_state.old_values.erase(obj.second.id);
                  continue;
               }
               // del + del -> N/A
               assert( prev_state.removed_values.find( obj.second.id ) == prev_state.removed_values.end() );
               // nop + del(was=Y) -> del(was=Y)
               prev_state.removed_values.emplace( std::move(obj) ); //[obj.second->id] = std::move(obj.second);
            }

            _stack.pop_back();
            squash_cache();
            --_revision;
         }

         /**
          * Discards all undo history prior to revision
          */
         void commit( int64_t revision )
         {
            while( _stack.size() && _stack[0].revision <= revision )
            {
               _stack.pop_front();
            }
         }

         void process_cache()
         {
             if ( !_cache_on|| !_cache.size()) return;
             auto& head= _cache.front();

             for(auto item = head.removed_ids.begin();item != head.removed_ids.end() && !_backup_indices.empty();)
             {
                 auto itr = _backup_indices.find( *item );
                 if(itr != _backup_indices.end()){
                     _backup_indices.erase( itr);
                     item = head.removed_ids.erase(item);
                     continue;
                 } else {
                     BOOST_THROW_EXCEPTION( std::logic_error( "process_cache: in removed_ids, Could not find object!!" ) );
                  }
                 ++item;
             }

             for(auto item = head.modify_values.begin();item != head.modify_values.end() && !_backup_indices.empty();)
             {
                 auto itr = _backup_indices.find(item->second.id);
                 if(itr != _backup_indices.end()){
                     auto ok = _backup_indices.modify( itr , [&]( value_type& v ) {
                                     v = std::move( item->second );
                                             });
                     if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "process_cache: Could not modify object, most likely a nstraint was violated" ) );
                      item = head.modify_values.erase(item);
                      continue;
                 } else {
                     BOOST_THROW_EXCEPTION( std::logic_error( "process_cache: in modify_values, Could not find object!!" ) );
                  }
                 ++item;
             }

             for(auto item = head.new_values.begin();item != head.new_values.end();)
             {

                 bool ok = _backup_indices.emplace( std::move( item->second ) ).second;
                 if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "process_cache: Could not restore object, most likely a nstraint was violated" ) );
                 item =  head.new_values.erase(item);
             }

             _cache.pop_front();
         }

         bool process_table()
         {
            if ( !_cache_on|| !_cache.size()) return false;
            auto& head = _cache.front();

            if (_backup_indices.size() == 0) BOOST_THROW_EXCEPTION( std::logic_error( "process_table: NO any record in process_table!!" ) );
            auto row = _backup_indices.begin();

             if (head.removed_ids.count(row->id) )
             {
                 head.removed_ids.erase(row->id);
                 _backup_indices.erase( row );
                 return true;
             }

             auto itr = head.modify_values.find(row->id);
             if (itr != head.modify_values.end() )
             {
                 auto ok = _backup_indices.modify( row, [&]( value_type& v ) {
                                          v = std::move( itr->second );
                                                           });
                 if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "process_table: Could not modify object, most likely a queness constraint was violated" ) );
                 head.modify_values.erase(row->id);
                 return false;
             }
             return false;
         }

         template<typename C>
         void process_table(C&& c)
         {
             if ( !_cache_on|| !_cache.size()) return;
             auto& head= _cache.front();

             for(auto item = head.removed_ids.begin();item != head.removed_ids.end() && !_backup_indices.empty();)
             {
                 auto itr = _backup_indices.find( *item );
                 if(itr != _backup_indices.end()){
                     _backup_indices.erase( itr);
                     item = head.removed_ids.erase(item);
                     continue;
                 }
                 ++item;
             }

             for(auto item = head.modify_values.begin();item != head.modify_values.end() && !_backup_indices.empty();)
             {
                 auto itr = _backup_indices.find(item->second.id);
                 if(itr != _backup_indices.end()){
                     auto ok = _backup_indices.modify( itr , [&]( value_type& v ) {
                         v = std::move( item->second );
                     });
                     if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "process_table-1: Could not modify object, most likely a nstraint was violated" ) );
                     item = head.modify_values.erase(item);
                     continue;
                 }
                 ++item;
             }

             for(auto item = head.new_values.begin();item != head.new_values.end();)
             {
                 if(c(*item)){
                     bool ok = _backup_indices.emplace( std::move( item->second ) ).second;
                     if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "process_table-1: Could not restore object, most likely a nstraint was violated" ) );
                     item = head.new_values.erase(item);
                     continue;
                 }
                 ++item;
             }
         }

         std::pair<bool,typename value_type::id_type> process_create()
         {
             if ( !_cache_on|| !_cache.size()) return {false,0};
             auto& create= _cache.front().new_values;

             if (create.empty()) return {false,0};

             auto item = create.begin();
             auto id=item->second.id;


             bool ok = _backup_indices.emplace( std::move(item->second)).second;;
             if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "process_create: Could not restore object, most likely a nstraint was violated" ) );
             create.erase(item->first);
             return {true, id};
         }
         /**
          * Unwinds all undo states
          */
         void undo_all()
         {
            while( enabled() )
               undo();
         }

         void set_revision( uint64_t revision )
         {
            if( _stack.size() != 0 )
               BOOST_THROW_EXCEPTION( std::logic_error("cannot set revision while there is an existing undo stack") );

            if( revision > std::numeric_limits<int64_t>::max() )
               BOOST_THROW_EXCEPTION( std::logic_error("revision to set is too high") );

            _revision = static_cast<int64_t>(revision);
            _cache.emplace_back( _indices.get_allocator() );
         }

         void remove_object( int64_t id )
         {
            const value_type* val = find( typename value_type::id_type(id) );
            if( !val ) BOOST_THROW_EXCEPTION( std::out_of_range( boost::lexical_cast<std::string>(id) ) );
            remove( *val );
         }

         std::pair<int64_t, int64_t> undo_stack_revision_range()const {
            int64_t begin = _revision;
            int64_t end   = _revision;

            if( _stack.size() > 0 ) {
               begin = _stack.front().revision - 1;
               end   = _stack.back().revision;
            }

            return {begin, end};
         }

         const auto& stack()const { return _stack; }
         auto& cache() { return _cache; }

      private:
         bool enabled()const { return _stack.size(); }

         void on_modify( const value_type& v ) {
            if( !enabled() ) return;

            auto& head = _stack.back();

            if( head.new_ids.find( v.id ) != head.new_ids.end() )
               return;

            auto itr = head.old_values.find( v.id );
            if( itr != head.old_values.end() )
               return;

            head.old_values.emplace( std::pair< typename value_type::id_type, const value_type& >( v.id, v ) );
         }

         void on_remove( const value_type& v ) {
            if( !enabled() ) return;

            auto& head = _stack.back();
            if( head.new_ids.count(v.id) ) {
               head.new_ids.erase( v.id );
               return;
            }

            auto itr = head.old_values.find( v.id );
            if( itr != head.old_values.end() ) {
               head.removed_values.emplace( std::move( *itr ) );
               head.old_values.erase( v.id );
               return;
            }

            if( head.removed_values.count( v.id ) )
               return;

            head.removed_values.emplace( std::pair< typename value_type::id_type, const value_type& >( v.id, v ) );
         }

         void on_create( const value_type& v ) {
            if( !enabled() ) return;
            auto& head = _stack.back();

            head.new_ids.insert( v.id );
         }

         void cache_remove( const value_type& v ) {
            if ( !_cache_on|| !_cache.size() ) return;
            auto& head = _cache.back();
            if( !head.new_values.erase(v.id) ){
               head.modify_values.erase(v.id);
               head.removed_ids.insert(v.id);
            }
         }

         void cache_modify( const value_type& v ) {
            if ( !_cache_on|| !_cache.size() ) return;
            auto& head = _cache.back();
            auto it = head.new_values.find(v.id);
            if( it != head.new_values.end() )
            {
                it->second=v;
                return;
            }
            it = head.modify_values.find(v.id);
            if( it != head.modify_values.end() )
            {
                it->second=v;
                return;
            }
            head.modify_values.emplace( std::pair< typename value_type::id_type, const value_type& >( v.id, v ) );
         }

         void cache_create( const value_type& v ) {
            if ( !_cache_on|| !_cache.size() ) return;
            _cache.back().new_values.emplace( std::pair< typename value_type::id_type, const value_type& >( v.id, v ) );
         }


         boost::interprocess::deque< undo_state_type, allocator<undo_state_type> > _stack;
         boost::interprocess::deque< cache_state_type, allocator<cache_state_type> > _cache;
         /**
          *  Each new session increments the revision, a squash will decrement the revision by combining
          *  the two most recent revisions into one revision.
          *
          *  Commit will discard all revisions prior to the committed revision.
          */
         bool                            _ws = false;
         uint64_t                        _cache_on;
         int64_t                         _revision = 0;
         typename value_type::id_type    _next_id = 0;
         index_type                      _indices;
         mutable index_type              _backup_indices;
         uint32_t                        _size_of_value_type = 0;
         uint32_t                        _size_of_this = 0;
   };

   class abstract_session {
      public:
         virtual ~abstract_session(){};
         virtual void push(bool ws=false)= 0;
         virtual void squash()           = 0;
         virtual void undo()             = 0;
         virtual int64_t revision()const  = 0;
   };

   template<typename SessionType>
   class session_impl : public abstract_session
   {
      public:
         session_impl( SessionType&& s ):_session( std::move( s ) ){}

         virtual void push(bool ws=false) override{ _session.push(ws);  }
         virtual void squash() override{ _session.squash(); }
         virtual void undo() override  { _session.undo();  }
         virtual int64_t revision()const override  { return _session.revision();  }
      private:
         SessionType _session;
   };

   class abstract_index
   {
      public:
         abstract_index( void* i ):_idx_ptr(i){}
         virtual ~abstract_index(){}
         virtual void     set_revision( uint64_t revision ) = 0;
         virtual unique_ptr<abstract_session> start_undo_session( bool enabled ) = 0;

         virtual int64_t revision()const = 0;
         virtual void    undo()const = 0;
         virtual void    squash()const = 0;
         virtual void    commit( int64_t revision )const = 0;
         virtual void    undo_all()const = 0;
         virtual uint32_t type_id()const  = 0;
         virtual uint64_t row_count()const = 0;
         virtual const std::string& type_name()const = 0;
         virtual std::pair<int64_t, int64_t> undo_stack_revision_range()const = 0;

         virtual void remove_object( int64_t id ) = 0;

         void* get()const { return _idx_ptr; }
      private:
         void* _idx_ptr;
   };

   template<typename BaseIndex>
   class index_impl : public abstract_index {
      public:
         index_impl( BaseIndex& base ):abstract_index( &base ),_base(base){}

         virtual unique_ptr<abstract_session> start_undo_session( bool enabled ) override {
            return unique_ptr<abstract_session>(new session_impl<typename BaseIndex::session>( _base.start_undo_session( enabled ) ) );
         }

         virtual void     set_revision( uint64_t revision ) override { _base.set_revision( revision ); }
         virtual int64_t  revision()const  override { return _base.revision(); }
         virtual void     undo()const  override { _base.undo(); }
         virtual void     squash()const  override { _base.squash(); }
         virtual void     commit( int64_t revision )const  override { _base.commit(revision); }
         virtual void     undo_all() const override {_base.undo_all(); }
         virtual uint32_t type_id()const override { return BaseIndex::value_type::type_id; }
         virtual uint64_t row_count()const override { return _base.indices().size(); }
         virtual const std::string& type_name() const override { return BaseIndex_name; }
         virtual std::pair<int64_t, int64_t> undo_stack_revision_range()const override { return _base.undo_stack_revision_range(); }

         virtual void     remove_object( int64_t id ) override { return _base.remove_object( id ); }
      private:
         BaseIndex& _base;
         std::string BaseIndex_name = boost::core::demangle( typeid( typename BaseIndex::value_type ).name() );
   };

   template<typename IndexType>
   class index : public index_impl<IndexType> {
      public:
         index( IndexType& i ):index_impl<IndexType>( i ){}
   };


   class read_write_mutex_manager
   {
      public:
         read_write_mutex_manager()
         {
            _current_lock = 0;
         }

         ~read_write_mutex_manager(){}

         void next_lock()
         {
            _current_lock++;
            new( &_locks[ _current_lock % CHAINBASE_NUM_RW_LOCKS ] ) read_write_mutex();
         }

         read_write_mutex& current_lock()
         {
            return _locks[ _current_lock % CHAINBASE_NUM_RW_LOCKS ];
         }

         uint32_t current_lock_num()
         {
            return _current_lock;
         }

      private:
         std::array< read_write_mutex, CHAINBASE_NUM_RW_LOCKS >     _locks;
         std::atomic< uint32_t >                                    _current_lock;
   };


   /**
    *  This class
    */
   class database
   {
      public:
         enum open_flags {
            read_only     = 0,
            read_write    = 1
         };

         using database_index_row_count_multiset = std::multiset<std::pair<unsigned, std::string>>;

         database(const bfs::path& dir, open_flags write = read_only, uint64_t shared_file_size = 0, bool ws = false, bool allow_dirty = false);
         ~database();
         database(database&&) = default;
         database& operator=(database&&) = default;
         bool is_read_only() const { return _read_only; }
         void flush();
         void set_require_locking( bool enable_require_locking );

#ifdef CHAINBASE_CHECK_LOCKING
         void require_lock_fail( const char* method, const char* lock_type, const char* tname )const;

         void require_read_lock( const char* method, const char* tname )const
         {
            if( BOOST_UNLIKELY( _enable_require_locking & _read_only & (_read_lock_count > 0) ) )
               require_lock_fail(method, "read", tname);
         }

         void require_write_lock( const char* method, const char* tname )
         {
            if( BOOST_UNLIKELY( _enable_require_locking & (_write_lock_count > 0) ) )
               require_lock_fail(method, "write", tname);
         }
#endif

         struct session {
            public:
               session( session&& s ):_index_sessions( std::move(s._index_sessions) ),_revision( s._revision ){}
               session( vector<std::unique_ptr<abstract_session>>&& s ):_index_sessions( std::move(s) )
               {
                  if( _index_sessions.size() )
                     _revision = _index_sessions[0]->revision();
               }

               ~session() {
                  undo();
               }

               void push(bool ws=false)
               {
                  for( auto& i : _index_sessions ) i->push(ws);
                  _index_sessions.clear();
               }

               void squash()
               {
                  for( auto& i : _index_sessions ) i->squash();
                  _index_sessions.clear();
               }

               void undo()
               {
                  for( auto& i : _index_sessions ) i->undo();
                  _index_sessions.clear();
               }

               int64_t revision()const { return _revision; }

            private:
               friend class database;
               session(){}

               vector< std::unique_ptr<abstract_session> > _index_sessions;
               int64_t _revision = -1;
         };

         session start_undo_session( bool enabled );

         int64_t revision()const {
             if( _index_list.size() == 0 ) return -1;
             return _index_list[0]->revision();
         }

         void undo();
         void squash();
         void commit( int64_t revision );
         void undo_all();

         void set_revision( uint64_t revision )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK( "set_revision", uint64_t );
             for( auto i : _index_list ) i->set_revision( revision );
         }

         template<typename VarType>
         void save_data(std::string var_name, const VarType& var)  {
            std::string type_name = boost::core::demangle( typeid(VarType).name() )  + var_name;
            auto var_ptr = _segment->find< VarType >( type_name.c_str() ).first;
            if( !var_ptr ) {
               _segment->find_or_construct< VarType >( type_name.c_str() )(var);
            } else {
               *var_ptr = var;
            }
         }

         template<typename VarType>
         bool get_data(std::string var_name, VarType& var)  {
            std::string type_name = boost::core::demangle( typeid(VarType).name() )  + var_name;
            auto var_ptr = _segment->find< VarType >( type_name.c_str()).first;
            if( !var_ptr ) {
               return false;
            } else {
               var = *var_ptr;
               return  true;
            }
         }

         template<typename MultiIndexType>
         void add_index() {
            const uint16_t type_id = generic_index<MultiIndexType>::value_type::type_id;
            typedef generic_index<MultiIndexType>          index_type;
            typedef typename index_type::allocator_type    index_alloc;

            std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );

            if( !( _index_map.size() <= type_id || _index_map[ type_id ] == nullptr ) ) {
               BOOST_THROW_EXCEPTION( std::logic_error( type_name + "::type_id is already in use" ) );
            }

            index_type* idx_ptr = _segment->find< index_type >( type_name.c_str() ).first;
            bool first_time_adding = false;
            if( !idx_ptr ) {
               if( _read_only ) {
                  BOOST_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in read only database" ) );
               }
               first_time_adding = true;
               idx_ptr = _segment->construct< index_type >( type_name.c_str() )( index_alloc( _segment->get_segment_manager() ), _ws);
             }

             idx_ptr->validate();

            // Ensure the undo stack of added index is consistent with the other indices in the database
            if( _index_list.size() > 0 ) {
               auto expected_revision_range = _index_list.front()->undo_stack_revision_range();
               auto added_index_revision_range = idx_ptr->undo_stack_revision_range();

               if( added_index_revision_range.first != expected_revision_range.first ||
                   added_index_revision_range.second != expected_revision_range.second ) {

                  if( !first_time_adding ) {
                     BOOST_THROW_EXCEPTION( std::logic_error(
                        "existing index for " + type_name + " has an undo stack (revision range [" +
                        std::to_string(added_index_revision_range.first) + ", " + std::to_string(added_index_revision_range.second) +
                        "]) that is inconsistent with other indices in the database (revision range [" +
                        std::to_string(expected_revision_range.first) + ", " + std::to_string(expected_revision_range.second) +
                        "]); corrupted database?"
                     ) );
                  }

                  if( _read_only ) {
                     BOOST_THROW_EXCEPTION( std::logic_error(
                        "new index for " + type_name +
                        " requires an undo stack that is consistent with other indices in the database; cannot fix in read-only mode"
                     ) );
                  }

                  idx_ptr->set_revision( static_cast<uint64_t>(expected_revision_range.first) );
                  while( idx_ptr->revision() < expected_revision_range.second ) {
                     idx_ptr->start_undo_session(true).push();
                  }
               }
            }

            if( type_id >= _index_map.size() )
               _index_map.resize( type_id + 1 );

            auto new_index = new index<index_type>( *idx_ptr );
            _index_map[ type_id ].reset( new_index );
            _index_list.push_back( new_index );
         }

         auto get_segment_manager() -> decltype( ((bip::managed_mapped_file*)nullptr)->get_segment_manager()) {
            return _segment->get_segment_manager();
         }

         auto get_segment_manager()const -> std::add_const_t< decltype( ((bip::managed_mapped_file*)nullptr)->get_segment_manager() ) > {
            return _segment->get_segment_manager();
         }

         size_t get_free_memory()const
         {
            return _segment->get_segment_manager()->get_free_memory();
         }

         template<typename MultiIndexType>
         const generic_index<MultiIndexType>& get_index()const
         {
            CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
            typedef generic_index<MultiIndexType> index_type;
            typedef index_type*                   index_type_ptr;
            assert( _index_map.size() > index_type::value_type::type_id );
            assert( _index_map[index_type::value_type::type_id] );
            return *index_type_ptr( _index_map[index_type::value_type::type_id]->get() );
         }

         template<typename MultiIndexType, typename ByIndex>
         auto get_index()const -> decltype( ((generic_index<MultiIndexType>*)( nullptr ))->indices().template get<ByIndex>() )
         {
            CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
            typedef generic_index<MultiIndexType> index_type;
            typedef index_type*                   index_type_ptr;
            assert( _index_map.size() > index_type::value_type::type_id );
            assert( _index_map[index_type::value_type::type_id] );
            return index_type_ptr( _index_map[index_type::value_type::type_id]->get() )->indices().template get<ByIndex>();
         }

         template<typename MultiIndexType>
         generic_index<MultiIndexType>& get_mutable_index()
         {
            CHAINBASE_REQUIRE_WRITE_LOCK("get_mutable_index", typename MultiIndexType::value_type);
            typedef generic_index<MultiIndexType> index_type;
            typedef index_type*                   index_type_ptr;
            assert( _index_map.size() > index_type::value_type::type_id );
            assert( _index_map[index_type::value_type::type_id] );
            return *index_type_ptr( _index_map[index_type::value_type::type_id]->get() );
         }

         template< typename ObjectType, typename IndexedByType, typename CompatibleKey >
         const ObjectType* find( CompatibleKey&& key )const
         {
             CHAINBASE_REQUIRE_READ_LOCK("find", ObjectType);
             typedef typename get_index_type< ObjectType >::type index_type;
             const auto& idx = get_index< index_type >().indices().template get< IndexedByType >();
             auto itr = idx.find( std::forward< CompatibleKey >( key ) );
             if( itr == idx.end() ) return nullptr;
             return &*itr;
         }

         template< typename ObjectType >
         const ObjectType* find( oid< ObjectType > key = oid< ObjectType >() ) const
         {
             CHAINBASE_REQUIRE_READ_LOCK("find", ObjectType);
             typedef typename get_index_type< ObjectType >::type index_type;
             const auto& idx = get_index< index_type >().indices();
             auto itr = idx.find( key );
             if( itr == idx.end() ) return nullptr;
             return &*itr;
         }

         template< typename ObjectType, typename IndexedByType, typename CompatibleKey >
         const ObjectType& get( CompatibleKey&& key )const
         {
             CHAINBASE_REQUIRE_READ_LOCK("get", ObjectType);
             auto obj = find< ObjectType, IndexedByType >( std::forward< CompatibleKey >( key ) );
             if( !obj ) {
                std::stringstream ss;
                ss << "unknown key (" << boost::core::demangle( typeid( key ).name() ) << "): " << key;
                BOOST_THROW_EXCEPTION( std::out_of_range( ss.str().c_str() ) );
             }
             return *obj;
         }

         template< typename ObjectType >
         const ObjectType& get( const oid< ObjectType >& key = oid< ObjectType >() )const
         {
             CHAINBASE_REQUIRE_READ_LOCK("get", ObjectType);
             auto obj = find< ObjectType >( key );
             if( !obj ) {
                std::stringstream ss;
                ss << "unknown key (" << boost::core::demangle( typeid( key ).name() ) << "): " << key._id;
                BOOST_THROW_EXCEPTION( std::out_of_range( ss.str().c_str() ) );
             }
             return *obj;
         }

         template<typename ObjectType, typename Modifier>
         void modify( const ObjectType& obj, Modifier&& m )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK("modify", ObjectType);
             typedef typename get_index_type<ObjectType>::type index_type;
             get_mutable_index<index_type>().modify( obj, m );
         }

         template<typename ObjectType>
         void remove( const ObjectType& obj )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK("remove", ObjectType);
             typedef typename get_index_type<ObjectType>::type index_type;
             return get_mutable_index<index_type>().remove( obj );
         }

         template<typename ObjectType, typename Constructor>
         const ObjectType& create( Constructor&& con )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK("create", ObjectType);
             typedef typename get_index_type<ObjectType>::type index_type;
             return get_mutable_index<index_type>().emplace( std::forward<Constructor>(con) );
         }

         template<typename ObjectType, typename Constructor>
         const ObjectType& backup_create( Constructor&& con )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK("backup_create", ObjectType);
             typedef typename get_index_type<ObjectType>::type index_type;
             return get_mutable_index<index_type>().backup_emplace( std::forward<Constructor>(con) );
         }

         template< typename Lambda >
         auto with_read_lock( Lambda&& callback, uint64_t wait_micro = 1000000 ) const -> decltype( (*(Lambda*)nullptr)() )
         {
            read_lock lock( _rw_manager->current_lock(), bip::defer_lock_type() );
#ifdef CHAINBASE_CHECK_LOCKING
            BOOST_ATTRIBUTE_UNUSED
            int_incrementer ii( _read_lock_count );
#endif

            if( !wait_micro )
            {
               lock.lock();
            }
            else
            {

               if( !lock.timed_lock( boost::posix_time::microsec_clock::local_time() + boost::posix_time::microseconds( wait_micro ) ) )
                  BOOST_THROW_EXCEPTION( std::runtime_error( "unable to acquire lock" ) );
            }

            return callback();
         }

         template< typename Lambda >
         auto with_write_lock( Lambda&& callback, uint64_t wait_micro = 1000000 ) -> decltype( (*(Lambda*)nullptr)() )
         {
            if( _read_only )
               BOOST_THROW_EXCEPTION( std::logic_error( "cannot acquire write lock on read-only process" ) );

            write_lock lock( _rw_manager->current_lock(), boost::defer_lock_t() );
#ifdef CHAINBASE_CHECK_LOCKING
            BOOST_ATTRIBUTE_UNUSED
            int_incrementer ii( _write_lock_count );
#endif

            if( !wait_micro )
            {
               lock.lock();
            }
            else
            {
               while( !lock.timed_lock( boost::posix_time::microsec_clock::local_time() + boost::posix_time::microseconds( wait_micro ) ) )
               {
                  _rw_manager->next_lock();
                  std::cerr << "Lock timeout, moving to lock " << _rw_manager->current_lock_num() << std::endl;
                  lock = write_lock( _rw_manager->current_lock(), boost::defer_lock_t() );
               }
            }

            return callback();
         }

         database_index_row_count_multiset row_count_per_index()const {
            database_index_row_count_multiset ret;
            for(const auto& ai_ptr : _index_map) {
               if(!ai_ptr)
                  continue;
               ret.emplace(make_pair(ai_ptr->row_count(), ai_ptr->type_name()));
            }
            return ret;
         }

      private:
         unique_ptr<bip::managed_mapped_file>                        _segment;
         unique_ptr<bip::managed_mapped_file>                        _meta;
         read_write_mutex_manager*                                   _rw_manager = nullptr;
         bool                                                        _read_only = false;
         bip::file_lock                                              _flock;

         /**
          * This is a sparse list of known indices kept to accelerate creation of undo sessions
          */
         vector<abstract_index*>                                     _index_list;

         /**
          * This is a full map (size 2^16) of all possible index designed for constant time lookup
          */
         vector<unique_ptr<abstract_index>>                          _index_map;

         bfs::path                                                   _data_dir;

#ifdef CHAINBASE_CHECK_LOCKING
         int32_t                                                     _read_lock_count = 0;
         int32_t                                                     _write_lock_count = 0;
         bool                                                        _enable_require_locking = false;
#endif
         bool                                                        _ws;
         void                                                        _msync_database();
   };

   template<typename Object, typename... Args>
   using shared_multi_index_container = boost::multi_index_container<Object,Args..., chainbase::allocator<Object> >;
}  // namepsace chainbase
