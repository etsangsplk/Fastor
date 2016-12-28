#ifndef REDUCIBLE_CONTRACTION_H
#define REDUCIBLE_CONTRACTION_H

#include "tensor/Tensor.h"
#include "indicial.h"


namespace Fastor {

// Broadcast-vectorisable contractions (if last dimension is contracted).
// Requres working on general strides


//----------------------------------------------------------------------------------------------------------------
// 4 word scalar
template<typename T, int ABI,
         typename std::enable_if<sizeof(T)==4 && ABI==32,bool>::type=0>
FASTOR_INLINE void vector_setter(SIMDVector<T,ABI> &vec, const T *data, int idx, int ) {
    vec.set(data[idx]);
}
// 4 word SSE
template<typename T, int ABI,
         typename std::enable_if<sizeof(T)==4 && ABI==128,bool>::type=0>
FASTOR_INLINE void vector_setter(SIMDVector<T,ABI> &vec, const T *data, int idx, int general_stride) {
    vec.set(data[idx+3*general_stride],data[idx+2*general_stride],data[idx+general_stride],data[idx]);
}
// 4 word AVX
template<typename T, int ABI,
         typename std::enable_if<sizeof(T)==4 && ABI==256,bool>::type=0>
FASTOR_INLINE void vector_setter(SIMDVector<T,ABI> &vec, const T *data, int idx, int general_stride) {
    vec.set(data[idx+7*general_stride],data[idx+6*general_stride],
            data[idx+5*general_stride],data[idx+4*general_stride],
            data[idx+3*general_stride],data[idx+2*general_stride],
            data[idx+general_stride],data[idx]);
}
// 4 word AVX 512
template<typename T, int ABI,
         typename std::enable_if<sizeof(T)==4 && ABI==512,bool>::type=0>
FASTOR_INLINE void vector_setter(SIMDVector<T,ABI> &vec, const T *data, int idx, int general_stride) {
    vec.set(data[idx+15*general_stride],data[idx+14*general_stride],
            data[idx+13*general_stride],data[idx+12*general_stride],
            data[idx+11*general_stride],data[idx+10*general_stride],
            data[idx+9*general_stride],data[idx+8*general_stride],
            data[idx+7*general_stride],data[idx+6*general_stride],
            data[idx+5*general_stride],data[idx+4*general_stride],
            data[idx+3*general_stride],data[idx+2*general_stride],
            data[idx+general_stride],data[idx]);
}

// 8 word scalar
template<typename T, int ABI,
         typename std::enable_if<sizeof(T)==8 && ABI==64,bool>::type=0>
FASTOR_INLINE void vector_setter(SIMDVector<T,ABI> &vec, const T *data, int idx, int ) {
    vec.set(data[idx]);
}
// 8 word SSE
template<typename T, int ABI,
         typename std::enable_if<sizeof(T)==8 && ABI==128,bool>::type=0>
FASTOR_INLINE void vector_setter(SIMDVector<T,ABI> &vec, const T *data, int idx, int general_stride) {
    vec.set(data[idx+general_stride],data[idx]);
}
// 8 word AVX
template<typename T, int ABI,
         typename std::enable_if<sizeof(T)==8 && ABI==256,bool>::type=0>
FASTOR_INLINE void vector_setter(SIMDVector<T,ABI> &vec, const T *data, int idx, int general_stride) {
    vec.set(data[idx+3*general_stride],data[idx+2*general_stride],
            data[idx+general_stride],data[idx]);
}
// 8 word AVX 512
template<typename T, int ABI,
         typename std::enable_if<sizeof(T)==8 && ABI==512,bool>::type=0>
FASTOR_INLINE void vector_setter(SIMDVector<T,ABI> &vec, const T *data, int idx, int general_stride) {
    vec.set(data[idx+7*general_stride],data[idx+6*general_stride],
            data[idx+5*general_stride],data[idx+4*general_stride],
            data[idx+3*general_stride],data[idx+2*general_stride],
            data[idx+general_stride],data[idx]);
}
//----------------------------------------------------------------------------------------------------------------








template<class T, class U>
struct extractor_reducible_contract {};

template<size_t ... Idx0, size_t ... Idx1>
struct extractor_reducible_contract<Index<Idx0...>, Index<Idx1...>> {


#if CONTRACT_OPT==2

    template<typename T, size_t ... Rest0, size_t ... Rest1>
      static
      typename contraction_impl<Index<Idx0...,Idx1...>, Tensor<T,Rest0...,Rest1...>,
               typename std_ext::make_index_sequence<sizeof...(Rest0)+sizeof...(Rest1)>::type>::type
      contract_impl(const Tensor<T,Rest0...> &a, const Tensor<T,Rest1...> &b) {

          static_assert(!is_reduction<Index<Idx0...>,Index<Idx1...>>::value,"REDUCTION TO SCALAR REQUESTED. USE REDUCTION FUNCTION INSTEAD");

          constexpr int total = no_of_loops_to_set<Index<Idx0...>,Index<Idx1...>,Tensor<T,Rest0...>,Tensor<T,Rest1...>,
                  typename std_ext::make_index_sequence<no_of_unique<Idx0...,Idx1...>::value>::type>::value;

          using index_generator = contract_meta_engine<Index<Idx0...>,Index<Idx1...>,Tensor<T,Rest0...>,Tensor<T,Rest1...>,
              typename std_ext::make_index_sequence<total>::type>;

          using OutTensor = typename index_generator::OutTensor;
          using OutIndices = typename index_generator::OutIndices;

          constexpr auto& index_a = index_generator::index_a;
          constexpr auto& index_b = index_generator::index_b;
          constexpr auto& index_out = index_generator::index_out;

          OutTensor out;
          out.zeros();
          const T *a_data = a.data();
          const T *b_data = b.data();
          T *out_data = out.data();

          // Check for reducible vectorisability
          constexpr int general_stride = general_stride_finder<Index<Idx0...>,Index<Idx1...>,
                  Tensor<T,Rest0...>,Tensor<T,Rest1...>, typename std_ext::make_index_sequence<sizeof...(Rest1)>::type>::value;


#ifndef FASTOR_DONT_VECTORISE
          using vectorisability = is_reducibly_vectorisable<OutIndices,OutTensor>;
          constexpr int stride = vectorisability::stride;
          using V = typename vectorisability::type;
#else
          constexpr int stride = 1;
          using V = SIMDVector<T,sizeof(T)*8>;
#endif

          V _vec_a, _vec_b;
          for (int i = 0; i < total; i+=stride) {
              _vec_a.set(*(a_data+index_a[i]));
              const auto idx_b = index_b[i];
              vector_setter(_vec_b, b_data, idx_b, general_stride);
              V _vec_out = _vec_a*_vec_b +  V(out_data+index_out[i]);
              _vec_out.store(out_data+index_out[i]);
          }

        return out;
    }


#elif CONTRACT_OPT==1

    template<typename T, size_t ... Rest0, size_t ... Rest1>
    static
    typename contraction_impl<Index<Idx0...,Idx1...>, Tensor<T,Rest0...,Rest1...>,
             typename std_ext::make_index_sequence<sizeof...(Rest0)+sizeof...(Rest1)>::type>::type
    contract_impl(const Tensor<T,Rest0...> &a, const Tensor<T,Rest1...> &b) {

      static_assert(!is_reduction<Index<Idx0...>,Index<Idx1...>>::value,"REDUCTION TO SCALAR REQUESTED. USE REDUCTION FUNCTION INSTEAD");

      using OutTensor = typename contraction_impl<Index<Idx0...,Idx1...>, Tensor<T,Rest0...,Rest1...>,
        typename std_ext::make_index_sequence<sizeof...(Rest0)+sizeof...(Rest1)>::type>::type;
      using OutIndices = typename contraction_impl<Index<Idx0...,Idx1...>, Tensor<T,Rest0...,Rest1...>,
        typename std_ext::make_index_sequence<sizeof...(Rest0)+sizeof...(Rest1)>::type>::indices;

      OutTensor out;
      out.zeros();
      const T *a_data = a.data();
      const T *b_data = b.data();
      T *out_data = out.data();

      constexpr int a_dim = sizeof...(Rest0);
      constexpr int b_dim = sizeof...(Rest1);

      constexpr auto& idx_a = IndexFirstTensor<Index<Idx0...>,Index<Idx1...>, Tensor<T,Rest0...>,Tensor<T,Rest1...>,
                                typename std_ext::make_index_sequence<sizeof...(Rest0)>::type>::indices;
      constexpr auto& idx_b = IndexSecondTensor<Index<Idx0...>,Index<Idx1...>, Tensor<T,Rest0...>,Tensor<T,Rest1...>,
                                typename std_ext::make_index_sequence<sizeof...(Rest1)>::type>::indices;
      constexpr auto& idx_out = IndexResultingTensor<Index<Idx0...>,Index<Idx1...>, Tensor<T,Rest0...>,Tensor<T,Rest1...>,
                                typename std_ext::make_index_sequence<OutTensor::Dimension>::type>::indices;

      constexpr int total = no_of_loops_to_set<Index<Idx0...>,Index<Idx1...>,Tensor<T,Rest0...>,Tensor<T,Rest1...>,
              typename std_ext::make_index_sequence<no_of_unique<Idx0...,Idx1...>::value>::type>::value;

      using maxes_out_type = typename no_of_loops_to_set<Index<Idx0...>,Index<Idx1...>,Tensor<T,Rest0...>,Tensor<T,Rest1...>,
              typename std_ext::make_index_sequence<no_of_unique<Idx0...,Idx1...>::value>::type>::type;

      constexpr auto& as_all = cartesian_product<maxes_out_type,typename std_ext::make_index_sequence<total>::type>::values;

      constexpr std::array<size_t,a_dim> products_a = nprods<Index<Rest0...>,typename std_ext::make_index_sequence<a_dim>::type>::values;
      constexpr std::array<size_t,b_dim> products_b = nprods<Index<Rest1...>,typename std_ext::make_index_sequence<b_dim>::type>::values;
      using Index_with_dims = typename put_dims_in_Index<OutTensor>::type;
      constexpr std::array<size_t,Index_with_dims::NoIndices> products_out = nprods<Index_with_dims,
              typename std_ext::make_index_sequence<Index_with_dims::NoIndices>::type>::values;


      // Check for reducible vectorisability
      constexpr int general_stride = general_stride_finder<Index<Idx0...>,Index<Idx1...>,
              Tensor<T,Rest0...>,Tensor<T,Rest1...>, typename std_ext::make_index_sequence<b_dim>::type>::value;


#ifndef FASTOR_DONT_VECTORISE
      using vectorisability = is_reducibly_vectorisable<OutIndices,OutTensor>;
      constexpr int stride = vectorisability::stride;
      using V = typename vectorisability::type;
#else
      constexpr int stride = 1;
      using V = SIMDVector<T,sizeof(T)*8>;
#endif

      int it;
      V _vec_a, _vec_b;

      for (int i = 0; i < total; i+=stride) {
          int index_a = as_all[i][idx_a[a_dim-1]];
          for(it = 0; it< a_dim; it++) {
              index_a += products_a[it]*as_all[i][idx_a[it]];
          }

          int index_b = as_all[i][idx_b[b_dim-1]];
          for(it = 0; it< b_dim; it++) {
              index_b += products_b[it]*as_all[i][idx_b[it]];
          }
          int index_out = as_all[i][idx_out[idx_out.size()-1]];
          for(it = 0; it< idx_out.size(); it++) {
              index_out += products_out[it]*as_all[i][idx_out[it]];
          }

          _vec_a.set(*(a_data+index_a));
//          _vec_b.set(b_data[index_b+3*general_stride], b_data[index_b+2*general_stride],
//                  b_data[index_b+general_stride],b_data[index_b]);
          vector_setter(_vec_b, b_data, index_b, general_stride);
          V _vec_out = _vec_a*_vec_b +  V(out_data+index_out);
          _vec_out.store(out_data+index_out);
      }

      return out;
    }


#else

    template<typename T, size_t ... Rest0, size_t ... Rest1>
      static
      typename contraction_impl<Index<Idx0...,Idx1...>, Tensor<T,Rest0...,Rest1...>,
               typename std_ext::make_index_sequence<sizeof...(Rest0)+sizeof...(Rest1)>::type>::type
      contract_impl(const Tensor<T,Rest0...> &a, const Tensor<T,Rest1...> &b) {

        static_assert(!is_reduction<Index<Idx0...>,Index<Idx1...>>::value,"REDUCTION TO SCALAR REQUESTED. USE REDUCTION FUNCTION INSTEAD");

        using OutTensor = typename contraction_impl<Index<Idx0...,Idx1...>, Tensor<T,Rest0...,Rest1...>,
          typename std_ext::make_index_sequence<sizeof...(Rest0)+sizeof...(Rest1)>::type>::type;
        using OutIndice = typename contraction_impl<Index<Idx0...,Idx1...>, Tensor<T,Rest0...,Rest1...>,
          typename std_ext::make_index_sequence<sizeof...(Rest0)+sizeof...(Rest1)>::type>::indices;

        OutTensor out;
        out.zeros();
        const T *a_data = a.data();
        const T *b_data = b.data();
        T *out_data = out.data();

        constexpr int a_dim = sizeof...(Rest0);
        constexpr int b_dim = sizeof...(Rest1);
        constexpr int out_dim =  no_of_unique<Idx0...,Idx1...>::value;

        constexpr auto& idx_a = IndexTensors<
                Index<Idx0..., Idx1...>,
                Tensor<T,Rest0...,Rest1...>,
                Index<Idx0...>,Tensor<T,Rest0...>,
                typename std_ext::make_index_sequence<sizeof...(Rest0)>::type>::indices;

        constexpr auto& idx_b = IndexTensors<
                Index<Idx0..., Idx1...>,
                Tensor<T,Rest0...,Rest1...>,
                Index<Idx1...>,Tensor<T,Rest1...>,
                typename std_ext::make_index_sequence<sizeof...(Rest1)>::type>::indices;

        constexpr auto& idx_out = IndexTensors<
                Index<Idx0..., Idx1...>,
                Tensor<T,Rest0...,Rest1...>,
                OutIndice,OutTensor,
                typename std_ext::make_index_sequence<OutTensor::Dimension>::type>::indices;

        using nloops = loop_setter<
                  Index<Idx0...,Idx1...>,
                  Tensor<T,Rest0...,Rest1...>,
                  typename std_ext::make_index_sequence<out_dim>::type>;
        constexpr auto& maxes_out = nloops::dims;
        constexpr int total = nloops::value;

        constexpr std::array<size_t,a_dim> products_a = nprods<Index<Rest0...>,typename std_ext::make_index_sequence<a_dim>::type>::values;
        constexpr std::array<size_t,b_dim> products_b = nprods<Index<Rest1...>,typename std_ext::make_index_sequence<b_dim>::type>::values;

        using Index_with_dims = typename put_dims_in_Index<OutTensor>::type;
        constexpr std::array<size_t,OutTensor::Dimension> products_out = \
                nprods<Index_with_dims,typename std_ext::make_index_sequence<OutTensor::Dimension>::type>::values;

        // Check for reducible vectorisability
//        using nloops_out = loop_setter<
//                  OutIndice, OutTensor, typename std_ext::make_index_sequence<OutTensor::Dimension>::type>;
//        constexpr int total_contracted = nloops_out::value;
//        constexpr int general_stride = total/total_contracted;
        constexpr int general_stride = general_stride_finder<Index<Idx0...>,Index<Idx1...>,
                Tensor<T,Rest0...>,Tensor<T,Rest1...>, typename std_ext::make_index_sequence<b_dim>::type>::value;

#ifndef FASTOR_DONT_VECTORISE
        using vectorisability = is_reducibly_vectorisable<OutIndice,OutTensor>;
        constexpr int stride = vectorisability::stride;
        using V = typename vectorisability::type;
#else
        constexpr int stride = 1;
        using V = SIMDVector<T,sizeof(T)*8>;
#endif

        int as[out_dim];
        std::fill(as,as+out_dim,0);

        int it;
        V _vec_a, _vec_b;

        for (int i = 0; i < total; i+=stride) {
            int remaining = total;
            for (int n = 0; n < out_dim; ++n) {
                remaining /= maxes_out[n];
                as[n] = ( i / remaining ) % maxes_out[n];
            }

            int index_a = as[idx_a[a_dim-1]];
            for(it = 0; it< a_dim; it++) {
                index_a += products_a[it]*as[idx_a[it]];
            }
            int index_b = as[idx_b[b_dim-1]];
            for(it = 0; it< b_dim; it++) {
                index_b += products_b[it]*as[idx_b[it]];
            }
            int index_out = as[idx_out[OutTensor::Dimension-1]];
            for(it = 0; it< static_cast<int>(OutTensor::Dimension); it++) {
                index_out += products_out[it]*as[idx_out[it]];
            }

    //        println(index_b, index_b+general_stride,index_b+2*general_stride, index_b+3*general_stride,"\n"); //
    //        println(index_out,index_a,index_b, index_b*general_stride,counter,"\n");
//            asm("#BEGIN");
            _vec_a.set(*(a_data+index_a));
//            _vec_a.broadcast(a_data+index_a);
            vector_setter(_vec_b, b_data, index_b, general_stride);
//            const T FASTOR_ALIGN tmp[] = {b_data[index_b+general_stride],b_data[index_b]};
//            _vec_b.load(tmp);
//            _vec_b.set(b_data[index_b+3*general_stride], b_data[index_b+2*general_stride],
//                    b_data[index_b+general_stride],b_data[index_b]);
            V _vec_out = _vec_a*_vec_b +  V(out_data+index_out);
            _vec_out.store(out_data+index_out);
//            asm("#END");
        }

        return out;
    }


#endif

};



template<class Index_I, class Index_J,
typename T, size_t ... Rest0, size_t ... Rest1>
auto strided_contraction(const Tensor<T,Rest0...> &a, const Tensor<T,Rest1...> &b)
-> decltype(extractor_reducible_contract<Index_I,Index_J>::contract_impl(a,b)) {
return extractor_reducible_contract<Index_I,Index_J>::contract_impl(a,b);
}

}


#endif // REDUCIBLE_CONTRACTION_H
