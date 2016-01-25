#pragma once

namespace triangle_stripper
{

  typedef u32 index;
  typedef std::vector<index> indices;

  enum primitive_type
  {
    TRIANGLES = 0x0004,     // = GL_TRIANGLES
    TRIANGLE_STRIP = 0x0005 // = GL_TRIANGLE_STRIP
  };

  struct primitive_group
  {
    indices Indices;
    primitive_type Type;
  };

  typedef std::vector<primitive_group> primitive_vector;

  namespace detail
  {
    class triangle
    {
    public:
      triangle() {}
      triangle(index A, index B, index C) : m_A(A), m_B(B), m_C(C), m_StripID(0) {}

      void ResetStripID() { m_StripID = 0; }
      void SetStripID(size_t StripID) { m_StripID = StripID; }
      size_t StripID() const { return m_StripID; }

      index A() const { return m_A; }
      index B() const { return m_B; }
      index C() const { return m_C; }

    private:
      index m_A;
      index m_B;
      index m_C;

      size_t m_StripID;
    };

    class triangle_edge
    {
    public:
      triangle_edge(index A, index B) : m_A(A), m_B(B) {}

      index A() const { return m_A; }
      index B() const { return m_B; }

      bool operator==(const triangle_edge& Right) const
      {
        return ((A() == Right.A()) && (B() == Right.B()));
      }

    private:
      index m_A;
      index m_B;
    };

    enum triangle_order
    {
      ABC,
      BCA,
      CAB
    };

    class strip
    {
    public:
      strip() : m_Start(0), m_Order(ABC), m_Size(0) {}

      strip(size_t Start, triangle_order Order, size_t Size)
          : m_Start(Start), m_Order(Order), m_Size(Size)
      {
      }

      size_t Start() const { return m_Start; }
      triangle_order Order() const { return m_Order; }
      size_t Size() const { return m_Size; }

    private:
      size_t m_Start;
      triangle_order m_Order;
      size_t m_Size;
    };

    class policy
    {
    public:
      policy(size_t MinStripSize, bool Cache);

      strip BestStrip() const;
      void Challenge(strip Strip, size_t Degree, size_t CacheHits);

    private:
      strip m_Strip;
      size_t m_Degree;
      size_t m_CacheHits;

      const size_t m_MinStripSize;
      const bool m_Cache;
    };

    inline policy::policy(size_t MinStripSize, bool Cache)
        : m_Degree(0), m_CacheHits(0), m_MinStripSize(MinStripSize), m_Cache(Cache)
    {
    }

    inline strip policy::BestStrip() const { return m_Strip; }

    // mutable heap
    // can be interfaced pretty muck like an array
    template <class T, class CmpT = std::less<T>>
    class heap_array
    {
    public:
      // Pre = PreCondition, Post = PostCondition

      heap_array() : m_Locked(false) {} // Post: ((size() == 0) && ! locked())

      void clear(); // Post: ((size() == 0) && ! locked())

      void reserve(size_t Size);
      size_t size() const;

      bool empty() const;
      bool locked() const;
      bool removed(size_t i) const; // Pre: (valid(i))
      bool valid(size_t i) const;

      size_t position(size_t i) const; // Pre: (valid(i))

      const T& top() const;                // Pre: (! empty())
      const T& peek(size_t i) const;       // Pre: (! removed(i))
      const T& operator[](size_t i) const; // Pre: (! removed(i))

      void lock();                // Pre: (! locked())   Post: (locked())
      size_t push(const T& Elem); // Pre: (! locked())

      void pop();                           // Pre: (locked() && ! empty())
      void erase(size_t i);                 // Pre: (locked() && ! removed(i))
      void update(size_t i, const T& Elem); // Pre: (locked() && ! removed(i))

    protected:
      heap_array(const heap_array&);
      heap_array& operator=(const heap_array&);

      class linker
      {
      public:
        linker(const T& Elem, size_t i) : m_Elem(Elem), m_Index(i) {}

        T m_Elem;
        size_t m_Index;
      };

      typedef std::vector<linker> linked_heap;
      typedef std::vector<size_t> finder;

      void Adjust(size_t i);
      void Swap(size_t a, size_t b);
      bool Less(const linker& a, const linker& b) const;

      linked_heap m_Heap;
      finder m_Finder;
      CmpT m_Compare;
      bool m_Locked;
    };

    //////////////////////////////////////////////////////////////////////////
    // heap_indexed inline functions
    //////////////////////////////////////////////////////////////////////////

    template <class T, class CmpT>
    inline void heap_array<T, CmpT>::clear()
    {
      m_Heap.clear();
      m_Finder.clear();
      m_Locked = false;
    }

    template <class T, class CmpT>
    inline bool heap_array<T, CmpT>::empty() const
    {
      return m_Heap.empty();
    }

    template <class T, class CmpT>
    inline bool heap_array<T, CmpT>::locked() const
    {
      return m_Locked;
    }

    template <class T, class CmpT>
    inline void heap_array<T, CmpT>::reserve(const size_t Size)
    {
      m_Heap.reserve(Size);
      m_Finder.reserve(Size);
    }

    template <class T, class CmpT>
    inline size_t heap_array<T, CmpT>::size() const
    {
      return m_Heap.size();
    }

    template <class T, class CmpT>
    inline const T& heap_array<T, CmpT>::top() const
    {
      assert(!empty());

      return m_Heap.front().m_Elem;
    }

    template <class T, class CmpT>
    inline const T& heap_array<T, CmpT>::peek(const size_t i) const
    {
      assert(!removed(i));

      return (m_Heap[m_Finder[i]].m_Elem);
    }

    template <class T, class CmpT>
    inline const T& heap_array<T, CmpT>::operator[](const size_t i) const
    {
      return peek(i);
    }

    template <class T, class CmpT>
    inline void heap_array<T, CmpT>::pop()
    {
      assert(locked());
      assert(!empty());

      Swap(0, size() - 1);
      m_Heap.pop_back();

      if (!empty())
        Adjust(0);
    }

    template <class T, class CmpT>
    inline void heap_array<T, CmpT>::lock()
    {
      assert(!locked());

      m_Locked = true;
    }

    template <class T, class CmpT>
    inline size_t heap_array<T, CmpT>::push(const T& Elem)
    {
      assert(!locked());

      const size_t Id = size();
      m_Finder.push_back(Id);
      m_Heap.push_back(linker(Elem, Id));
      Adjust(Id);

      return Id;
    }

    template <class T, class CmpT>
    inline void heap_array<T, CmpT>::erase(const size_t i)
    {
      assert(locked());
      assert(!removed(i));

      const size_t j = m_Finder[i];
      Swap(j, size() - 1);
      m_Heap.pop_back();

      if (j != size())
        Adjust(j);
    }

    template <class T, class CmpT>
    inline bool heap_array<T, CmpT>::removed(const size_t i) const
    {
      assert(valid(i));

      return (m_Finder[i] >= m_Heap.size());
    }

    template <class T, class CmpT>
    inline bool heap_array<T, CmpT>::valid(const size_t i) const
    {
      return (i < m_Finder.size());
    }

    template <class T, class CmpT>
    inline size_t heap_array<T, CmpT>::position(const size_t i) const
    {
      assert(valid(i));

      return (m_Heap[i].m_Index);
    }

    template <class T, class CmpT>
    inline void heap_array<T, CmpT>::update(const size_t i, const T& Elem)
    {
      assert(locked());
      assert(!removed(i));

      const size_t j = m_Finder[i];
      m_Heap[j].m_Elem = Elem;
      Adjust(j);
    }

    template <class T, class CmpT>
    inline void heap_array<T, CmpT>::Adjust(size_t i)
    {
      assert(i < m_Heap.size());

      size_t j;

      // Check the upper part of the heap
      for (j = i; (j > 0) && (Less(m_Heap[(j - 1) / 2], m_Heap[j])); j = ((j - 1) / 2))
        Swap(j, (j - 1) / 2);

      // Check the lower part of the heap
      for (i = j; (j = 2 * i + 1) < size(); i = j)
      {
        if ((j + 1 < size()) && (Less(m_Heap[j], m_Heap[j + 1])))
          ++j;

        if (Less(m_Heap[j], m_Heap[i]))
          return;

        Swap(i, j);
      }
    }

    template <class T, class CmpT>
    inline void heap_array<T, CmpT>::Swap(const size_t a, const size_t b)
    {
      std::swap(m_Heap[a], m_Heap[b]);

      m_Finder[(m_Heap[a].m_Index)] = a;
      m_Finder[(m_Heap[b].m_Index)] = b;
    }

    template <class T, class CmpT>
    inline bool heap_array<T, CmpT>::Less(const linker& a, const linker& b) const
    {
      return m_Compare(a.m_Elem, b.m_Elem);
    }

    // graph_array main class
    template <class nodetype>
    class graph_array
    {
    public:
      class arc;
      class node;

      // New types
      typedef size_t nodeid;
      typedef nodetype value_type;
      typedef std::vector<node> node_vector;
      typedef typename node_vector::iterator node_iterator;
      typedef typename node_vector::const_iterator const_node_iterator;
      typedef typename node_vector::reverse_iterator node_reverse_iterator;
      typedef typename node_vector::const_reverse_iterator const_node_reverse_iterator;

      typedef graph_array<nodetype> graph_type;

      // graph_array::arc class
      class arc
      {
      public:
        node_iterator terminal() const;

      protected:
        friend class graph_array<nodetype>;

        arc(node_iterator Terminal);

        node_iterator m_Terminal;
      };

      // New types
      typedef std::vector<arc> arc_list;
      typedef typename arc_list::iterator out_arc_iterator;
      typedef typename arc_list::const_iterator const_out_arc_iterator;

      // graph_array::node class
      class node
      {
      public:
        void mark();
        void unmark();
        bool marked() const;

        bool out_empty() const;
        size_t out_size() const;

        out_arc_iterator out_begin();
        out_arc_iterator out_end();
        const_out_arc_iterator out_begin() const;
        const_out_arc_iterator out_end() const;

        value_type& operator*();
        value_type* operator->();
        const value_type& operator*() const;
        const value_type* operator->() const;

        value_type& operator=(const value_type& Elem);

      protected:
        friend class graph_array<nodetype>;
        friend class std::vector<node>;

        node(arc_list& Arcs);

        arc_list& m_Arcs;
        size_t m_Begin;
        size_t m_End;

        value_type m_Elem;
        bool m_Marker;
      };

      graph_array();
      explicit graph_array(size_t NbNodes);

      // Node related member functions
      bool empty() const;
      size_t size() const;

      node& operator[](nodeid i);
      const node& operator[](nodeid i) const;

      node_iterator begin();
      node_iterator end();
      const_node_iterator begin() const;
      const_node_iterator end() const;

      node_reverse_iterator rbegin();
      node_reverse_iterator rend();
      const_node_reverse_iterator rbegin() const;
      const_node_reverse_iterator rend() const;

      // Arc related member functions
      out_arc_iterator insert_arc(nodeid Initial, nodeid Terminal);
      out_arc_iterator insert_arc(node_iterator Initial, node_iterator Terminal);

      // Optimized (overloaded) functions
      void swap(graph_type& Right);
      friend void swap(graph_type& Left, graph_type& Right) { Left.swap(Right); }

    protected:
      graph_array(const graph_type&);
      graph_type& operator=(const graph_type&);

      node_vector m_Nodes;
      arc_list m_Arcs;
    };

    // Additional "low level", graph related, functions
    template <class nodetype>
    void unmark_nodes(graph_array<nodetype>& G);

    //////////////////////////////////////////////////////////////////////////
    // graph_array::arc inline functions
    //////////////////////////////////////////////////////////////////////////

    template <class N>
    inline graph_array<N>::arc::arc(node_iterator Terminal)
        : m_Terminal(Terminal)
    {
    }

    template <class N>
    inline typename graph_array<N>::node_iterator graph_array<N>::arc::terminal() const
    {
      return m_Terminal;
    }

    //////////////////////////////////////////////////////////////////////////
    // graph_array::node inline functions
    //////////////////////////////////////////////////////////////////////////

    template <class N>
    inline graph_array<N>::node::node(arc_list& Arcs)
        : m_Arcs(Arcs)
        , m_Begin(std::numeric_limits<size_t>::max())
        , m_End(std::numeric_limits<size_t>::max())
        , m_Marker(false)
    {
    }

    template <class N>
    inline void graph_array<N>::node::mark()
    {
      m_Marker = true;
    }

    template <class N>
    inline void graph_array<N>::node::unmark()
    {
      m_Marker = false;
    }

    template <class N>
    inline bool graph_array<N>::node::marked() const
    {
      return m_Marker;
    }

    template <class N>
    inline bool graph_array<N>::node::out_empty() const
    {
      return (m_Begin == m_End);
    }

    template <class N>
    inline size_t graph_array<N>::node::out_size() const
    {
      return (m_End - m_Begin);
    }

    template <class N>
    inline typename graph_array<N>::out_arc_iterator graph_array<N>::node::out_begin()
    {
      return (m_Arcs.begin() + m_Begin);
    }

    template <class N>
    inline typename graph_array<N>::out_arc_iterator graph_array<N>::node::out_end()
    {
      return (m_Arcs.begin() + m_End);
    }

    template <class N>
    inline typename graph_array<N>::const_out_arc_iterator graph_array<N>::node::out_begin() const
    {
      return (m_Arcs.begin() + m_Begin);
    }

    template <class N>
    inline typename graph_array<N>::const_out_arc_iterator graph_array<N>::node::out_end() const
    {
      return (m_Arcs.begin() + m_End);
    }

    template <class N>
    inline N& graph_array<N>::node::operator*()
    {
      return m_Elem;
    }

    template <class N>
    inline N* graph_array<N>::node::operator->()
    {
      return &m_Elem;
    }

    template <class N>
    inline const N& graph_array<N>::node::operator*() const
    {
      return m_Elem;
    }

    template <class N>
    inline const N* graph_array<N>::node::operator->() const
    {
      return &m_Elem;
    }

    template <class N>
    inline N& graph_array<N>::node::operator=(const N& Elem)
    {
      return (m_Elem = Elem);
    }

    //////////////////////////////////////////////////////////////////////////
    // graph_array inline functions
    //////////////////////////////////////////////////////////////////////////

    template <class N>
    inline graph_array<N>::graph_array()
    {
    }

    template <class N>
    inline graph_array<N>::graph_array(const size_t NbNodes)
        : m_Nodes(NbNodes, node(m_Arcs))
    {
      // optimisation: we consider that, averagely, a triangle may have at least 2 neighbours
      // otherwise we are just wasting a bit of memory, but not that much
      m_Arcs.reserve(NbNodes * 2);
    }

    template <class N>
    inline bool graph_array<N>::empty() const
    {
      return m_Nodes.empty();
    }

    template <class N>
    inline size_t graph_array<N>::size() const
    {
      return m_Nodes.size();
    }

    template <class N>
    inline typename graph_array<N>::node& graph_array<N>::operator[](const nodeid i)
    {
      assert(i < size());

      return m_Nodes[i];
    }

    template <class N>
    inline const typename graph_array<N>::node& graph_array<N>::operator[](const nodeid i) const
    {
      assert(i < size());

      return m_Nodes[i];
    }

    template <class N>
    inline typename graph_array<N>::node_iterator graph_array<N>::begin()
    {
      return m_Nodes.begin();
    }

    template <class N>
    inline typename graph_array<N>::node_iterator graph_array<N>::end()
    {
      return m_Nodes.end();
    }

    template <class N>
    inline typename graph_array<N>::const_node_iterator graph_array<N>::begin() const
    {
      return m_Nodes.begin();
    }

    template <class N>
    inline typename graph_array<N>::const_node_iterator graph_array<N>::end() const
    {
      return m_Nodes.end();
    }

    template <class N>
    inline typename graph_array<N>::node_reverse_iterator graph_array<N>::rbegin()
    {
      return m_Nodes.rbegin();
    }

    template <class N>
    inline typename graph_array<N>::node_reverse_iterator graph_array<N>::rend()
    {
      return m_Nodes.rend();
    }

    template <class N>
    inline typename graph_array<N>::const_node_reverse_iterator graph_array<N>::rbegin() const
    {
      return m_Nodes.rbegin();
    }

    template <class N>
    inline typename graph_array<N>::const_node_reverse_iterator graph_array<N>::rend() const
    {
      return m_Nodes.rend();
    }

    template <class N>
    inline typename graph_array<N>::out_arc_iterator graph_array<N>::insert_arc(
        const nodeid Initial, const nodeid Terminal)
    {
      assert(Initial < size());
      assert(Terminal < size());

      return insert_arc(m_Nodes.begin() + Initial, m_Nodes.begin() + Terminal);
    }

    template <class N>
    inline typename graph_array<N>::out_arc_iterator graph_array<N>::insert_arc(
        const node_iterator Initial, const node_iterator Terminal)
    {
      assert((Initial >= begin()) && (Initial < end()));
      assert((Terminal >= begin()) && (Terminal < end()));

      node& Node = *Initial;

      if (Node.out_empty())
      {

        Node.m_Begin = m_Arcs.size();
        Node.m_End = m_Arcs.size() + 1;
      }
      else
      {

        // we optimise here for make_connectivity_graph()
        // we know all the arcs for a given node are successively and sequentially added
        assert(Node.m_End == m_Arcs.size());

        ++(Node.m_End);
      }

      m_Arcs.push_back(arc(Terminal));

      out_arc_iterator it = m_Arcs.end();
      return (--it);
    }

    template <class N>
    inline void graph_array<N>::swap(graph_type& Right)
    {
      std::swap(m_Nodes, Right.m_Nodes);
      std::swap(m_Arcs, Right.m_Arcs);
    }

    void make_connectivity_graph(graph_array<triangle>& Triangles, const indices& Indices);

    //////////////////////////////////////////////////////////////////////////
    // additional functions
    //////////////////////////////////////////////////////////////////////////

    template <class N>
    inline void unmark_nodes(graph_array<N>& G)
    {
      std::for_each(G.begin(), G.end(), std::mem_fun_ref(&graph_array<N>::node::unmark));
    }

    class cache_simulator
    {
    public:
      cache_simulator();

      void clear();
      void resize(size_t Size);
      void reset();
      void push_cache_hits(bool Enabled = true);
      size_t size() const;

      void push(index i, bool CountCacheHit = false);
      void merge(const cache_simulator& Backward, size_t PossibleOverlap);

      void reset_hitcount();
      size_t hitcount() const;

    protected:
      typedef std::deque<index> indices_deque;

      indices_deque m_Cache;
      size_t m_NbHits;
      bool m_PushHits;
    };

    //////////////////////////////////////////////////////////////////////////
    // cache_simulator inline functions
    //////////////////////////////////////////////////////////////////////////

    inline cache_simulator::cache_simulator() : m_NbHits(0), m_PushHits(true) {}

    inline void cache_simulator::clear()
    {
      reset_hitcount();
      m_Cache.clear();
    }

    inline void cache_simulator::resize(const size_t Size)
    {
      m_Cache.resize(Size, std::numeric_limits<index>::max());
    }

    inline void cache_simulator::reset()
    {
      std::fill(m_Cache.begin(), m_Cache.end(), std::numeric_limits<index>::max());
      reset_hitcount();
    }

    inline void cache_simulator::push_cache_hits(bool Enabled) { m_PushHits = Enabled; }

    inline size_t cache_simulator::size() const { return m_Cache.size(); }

    inline void cache_simulator::push(const index i, const bool CountCacheHit)
    {
      if (CountCacheHit || m_PushHits)
      {

        if (std::find(m_Cache.begin(), m_Cache.end(), i) != m_Cache.end())
        {

          // Should we count the cache hits?
          if (CountCacheHit)
            ++m_NbHits;

          // Should we not push the index into the cache if it's a cache hit?
          if (!m_PushHits)
            return;
        }
      }

      // Manage the indices cache as a FIFO structure
      m_Cache.push_front(i);
      m_Cache.pop_back();
    }

    inline void cache_simulator::merge(
        const cache_simulator& Backward, const size_t PossibleOverlap)
    {
      const size_t Overlap = std::min(PossibleOverlap, size());

      for (size_t i = 0; i < Overlap; ++i)
        push(Backward.m_Cache[i], true);

      m_NbHits += Backward.m_NbHits;
    }

    inline void cache_simulator::reset_hitcount() { m_NbHits = 0; }

    inline size_t cache_simulator::hitcount() const { return m_NbHits; }

  } // namespace detail

  class tri_stripper
  {
  public:
    tri_stripper(const indices& TriIndices);

    void Strip(primitive_vector* out_pPrimitivesVector);

    /* Stripifier Algorithm Settings */

    // Set the post-T&L cache size (0 disables the cache optimizer).
    void SetCacheSize(size_t CacheSize = 10);

    // Set the minimum size of a triangle strip (should be at least 2 triangles).
    // The stripifier discard any candidate strips that does not satisfy the minimum size
    // condition.
    void SetMinStripSize(size_t MinStripSize = 2);

    // Set the backward search mode in addition to the forward search mode.
    // In forward mode, the candidate strips are build with the current candidate triangle being
    // the first
    // triangle of the strip. When the backward mode is enabled, the stripifier also tests
    // candidate strips
    // where the current candidate triangle is the last triangle of the strip.
    // Enable this if you want better results at the expense of being slightly slower.
    // Note: Do *NOT* use this when the cache optimizer is enabled; it only gives worse results.
    void SetBackwardSearch(bool Enabled = false);

    // Set the cache simulator FIFO behavior (does nothing if the cache optimizer is disabled).
    // When enabled, the cache is simulated as a simple FIFO structure. However, when
    // disabled, indices that trigger cache hits are not pushed into the FIFO structure.
    // This allows simulating some GPUs that do not duplicate cache entries (e.g. NV25 or
    // greater).
    void SetPushCacheHits(bool Enabled = true);

    /* End Settings */

  private:
    typedef detail::graph_array<detail::triangle> triangle_graph;
    typedef detail::heap_array<size_t, std::greater<size_t>> triangle_heap;
    typedef std::vector<size_t> candidates;
    typedef triangle_graph::node_iterator tri_iterator;
    typedef triangle_graph::const_node_iterator const_tri_iterator;
    typedef triangle_graph::out_arc_iterator link_iterator;
    typedef triangle_graph::const_out_arc_iterator const_link_iterator;

    void InitTriHeap();
    void Stripify();
    void AddLeftTriangles();
    void ResetStripIDs();

    detail::strip FindBestStrip();
    detail::strip ExtendToStrip(size_t Start, detail::triangle_order Order);
    detail::strip BackExtendToStrip(size_t Start, detail::triangle_order Order, bool ClockWise);
    const_link_iterator LinkToNeighbour(
        const_tri_iterator Node, bool ClockWise, detail::triangle_order& Order, bool NotSimulation);
    const_link_iterator BackLinkToNeighbour(
        const_tri_iterator Node, bool ClockWise, detail::triangle_order& Order);
    void BuildStrip(const detail::strip Strip);
    void MarkTriAsTaken(size_t i);
    void AddIndex(index i, bool NotSimulation);
    void BackAddIndex(index i);
    void AddTriangle(const detail::triangle& Tri, detail::triangle_order Order, bool NotSimulation);
    void BackAddTriangle(const detail::triangle& Tri, detail::triangle_order Order);

    bool Cache() const;
    size_t CacheSize() const;

    static detail::triangle_edge FirstEdge(
        const detail::triangle& Triangle, detail::triangle_order Order);
    static detail::triangle_edge LastEdge(
        const detail::triangle& Triangle, detail::triangle_order Order);

    primitive_vector m_PrimitivesVector;
    triangle_graph m_Triangles;
    triangle_heap m_TriHeap;
    candidates m_Candidates;
    detail::cache_simulator m_Cache;
    detail::cache_simulator m_BackCache;
    size_t m_StripID;
    size_t m_MinStripSize;
    bool m_BackwardSearch;
    bool m_FirstRun;
  };

  //////////////////////////////////////////////////////////////////////////
  // tri_stripper inline functions
  //////////////////////////////////////////////////////////////////////////

  inline void tri_stripper::SetCacheSize(const size_t CacheSize)
  {
    m_Cache.resize(CacheSize);
    m_BackCache.resize(CacheSize);
  }

  inline void tri_stripper::SetMinStripSize(const size_t MinStripSize)
  {
    if (MinStripSize < 2)
      m_MinStripSize = 2;
    else
      m_MinStripSize = MinStripSize;
  }

  inline void tri_stripper::SetBackwardSearch(const bool Enabled) { m_BackwardSearch = Enabled; }

  inline void tri_stripper::SetPushCacheHits(bool Enabled) { m_Cache.push_cache_hits(Enabled); }
}