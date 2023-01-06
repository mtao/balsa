#include <tuple>
#include <array>
#include <numeric>
#include <algorithm>
#include <highfive/H5DataType.hpp>
#include <highfive/H5File.hpp>
#include <highfive/H5DataSet.hpp>
#include <highfive/H5DataType.hpp>

// template <typename... Args>
// HighFive::CompoundType make_compound_datatype(std::span<std::string,sizeof...(Args)& names) {
//     std::array<std::pair<std::string_view,HighFive::DataType>>> types;
//
//     return HighFive::CompoundType(types);
//
// }

namespace wmtk {
template<typename T>
struct AttributeChange {
    size_t index;
    T old_value;
    T new_value;
};

}// namespace wmtk

/// \brief Macro to register an attribute type for highfive/hdf5
///
/// This macro has to be called outside of any namespace


namespace wmtk {


struct Operation {
    struct AttributeUpdate {
        std::string name;
        std::array<size_t, 2> data_range;

        static HighFive::CompoundType data_type();
    };
    std::string name;
    std::vector<size_t> operation_tuple;
    std::vector<AttributeUpdate> attribute_updates;

    static HighFive::CompoundType data_type();
};


HighFive::CompoundType
  Operation::AttributeUpdate::data_type() {

    return HighFive::CompoundType{ { "name", HighFive::create_datatype<size_t>() },
                                   { "data_range_start", HighFive::create_datatype<size_t>() },
                                   { "data_range_end", HighFive::create_datatype<size_t>() } };
}

HighFive::CompoundType
  Operation::data_type() {

    return HighFive::CompoundType{ { "name", HighFive::create_datatype<size_t>() },
                                   { "operation_tuple", HighFive::create_datatype<std::vector<size_t>>() },
                                   { "attribute_updates", HighFive::create_datatype<std::vector<AttributeUpdate>>() } };
}

template<typename T>
class Logger {
  public:
    using AttributeChangeType = AttributeChange<T>;

    Logger(HighFive::File &file, const std::string_view &name);

    // Basic means is to write one piece at a time
    void write(size_t index, T &&old, T &&new_value);
    void write(const AttributeChangeType &t);

    static HighFive::DataSetAccessProps access_properties();
    static HighFive::DataSetCreateProps create_properties();
    static HighFive::CompoundType data_type();

  private:
    HighFive::DataSet dataset;
};

template<typename T>
HighFive::CompoundType Logger<T>::data_type() {

    return HighFive::CompoundType{ { "index", HighFive::create_datatype<size_t>() },
                                   { "old_value", HighFive::create_datatype<T>() },
                                   { "new_value", HighFive::create_datatype<T>() } };
}
template<typename T>
HighFive::DataSetAccessProps Logger<T>::access_properties() {
    HighFive::DataSetAccessProps props;
    // props.add(HighFive::Chunking(std::vector<hsize_t>{ 2 }));
    return props;
}
template<typename T>
HighFive::DataSetCreateProps Logger<T>::create_properties() {
    HighFive::DataSetCreateProps props;
    props.add(HighFive::Chunking(std::vector<hsize_t>{ 2 }));
    return props;
}


template<typename T>
Logger<T>::Logger(HighFive::File &file, const std::string_view &name)
  : dataset(file.createDataSet(std::string(name),
                               HighFive::DataSpace({ 0 }, { HighFive::DataSpace::UNLIMITED }),
                               data_type(),
                               create_properties(),
                               access_properties())) {
}
template<typename T>
void Logger<T>::write(size_t index, T &&old, T &&new_value) {
    write({ index, old, new_value });
}
template<typename T>
void Logger<T>::write(const AttributeChangeType &t) {
    hsize_t insertion_element = dataset.getElementCount();
    dataset.resize({ insertion_element + 1 });
    // std::vector<AttributeChangeType> data;
    // data.emplace_back(t);
    // dataset.select(std::vector<hsize_t>{ insertion_element }).write(data);
    dataset.select(std::vector<hsize_t>{ insertion_element }).write_raw(&t);
}
}// namespace wmtk

#define WMTK_REGISTER_ATTRIBUTE_TYPE(type) \
    HIGHFIVE_REGISTER_TYPE(wmtk::AttributeChange<type>, wmtk::Logger<type>::data_type)

WMTK_REGISTER_ATTRIBUTE_TYPE(double);

#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
TEST_CASE("compound type", "[hdf5,highfive]") {
    using namespace HighFive;
    File file("compount_type.hd5", File::ReadWrite | File::Create | File::Truncate);


    wmtk::Logger<double> logger(file, "double_update");

    logger.write(0, 0, 1);
    logger.write(1, 2, 1);
    logger.write(1, 3, 1);
    logger.write(3, 4, 1);
    logger.write(4, 5, 1);
}
