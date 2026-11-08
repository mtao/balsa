#include "balsa/image/convert.hpp"

#include <zipper/Array.hpp>
#include <zipper/ArrayBase.hxx>
#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/as.hpp>
#include <zipper/expression/nullary/StaticConstant.hpp>
#include <zipper/expression/nullary/Zero.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace balsa::image {

namespace {
    template <typename T>
    concept Component = std::same_as<T, uint8_t> || std::same_as<T, uint16_t>
                        || std::same_as<T, float>;

    // The value of T meaning 1: its maximum for integers.
    template <Component T>
    constexpr float unit =
        std::is_integral_v<T>
            ? static_cast<float>(std::numeric_limits<T>::max())
            : 1.0f;

    // Normalised float values as T, rounded for integers.
    template <Component T>
    auto from_unit(const auto &values) {
        if constexpr (std::is_integral_v<T>) {
            return (::zipper::clamp(values, 0.0f, 1.0f) * unit<T>)
                .round()
                .template cast<T>();
        } else {
            return values.template cast<T>();
        }
    }

    // Rec. 709 luminance weights.
    constexpr std::array<float, 3> luminance{0.2126f, 0.7152f, 0.0722f};

    // The affine map from a pixel in `from` to one in `to`, in homogeneous
    // form: out = A [in; 1], an out x (in + 1) matrix whose last column is
    // the offset (an opaque alpha of value `alpha`, if added).
    auto channel_map(Channels from, Channels to, float alpha)
        -> ::zipper::MatrixXX<float> {
        const auto in = static_cast<std::size_t>(from);
        const auto out = static_cast<std::size_t>(to);
        const bool colour_in = in >= 3;
        const bool alpha_in = in == 2 || in == 4;
        using ::zipper::expression::nullary::Zero;
        constexpr auto dynamic = ::zipper::dynamic_extent;
        ::zipper::MatrixXX<float> A =
            Zero<float, dynamic, dynamic>(out, in + 1);
        if (out >= 3) {
            if (colour_in) {
                A.diagonal().head<3>().fill(1.0f);
            } else {
                A.col(0).head<3>().fill(1.0f);
            }
        } else if (colour_in) {
            A.row(0).head<3>() = ::zipper::VectorBase(luminance);
        } else {
            A(0, 0) = 1.0f;
        }
        if (out == 2 || out == 4) {
            // From the input's alpha, else the constant column.
            A(out - 1, alpha_in ? in - 1 : in) = alpha_in ? 1.0f : alpha;
        }
        return A;
    }

    template <Component T>
    void rearrange(const Image &image, Image &result) {
        const auto A = channel_map(*image.layout(), *result.layout(), unit<T>);
        const auto source = *image.array<T>();
        const auto mapped =
            ::zipper::as_array(source.template over<2>().template map<2>(
                [&](const auto &pixel) {
                    return A
                           * ::zipper::as_vector(pixel)
                                 .template cast<float>()
                                 .homogeneous();
                },
                ::zipper::dextents<1>(result.channels())));
        auto target = *result.array<T>();
        if constexpr (std::is_integral_v<T>) {
            target = mapped.round().template cast<T>();
        } else {
            target = mapped.template cast<T>();
        }
    }

    template <Component From, Component To>
    void recast(const Image &image, Image &result) {
        const auto source = *image.array<From>();
        auto target = *result.array<To>();
        target = from_unit<To>(source.template cast<float>() / unit<From>);
    }

    // Calls f.template operator()<T>() with T the C++ type of `component`.
    template <typename F>
    void with_type(ComponentType component, F &&f) {
        switch (component) {
        case ComponentType::UInt8:
            f.template operator()<uint8_t>();
            break;
        case ComponentType::UInt16:
            f.template operator()<uint16_t>();
            break;
        case ComponentType::Float32:
            f.template operator()<float>();
            break;
        }
    }
} // namespace

auto convert_channels(const Image &image, Channels layout) -> Image {
    if (!image.layout()) {
        throw std::invalid_argument(
            "image::convert_channels: the image's layout is unknown");
    }
    if (image.layout() == layout) return image;
    Image result(image.height(), image.width(), layout, image.component());
    with_type(image.component(),
              [&]<Component T>() { rearrange<T>(image, result); });
    return result;
}

auto convert_component(const Image &image, ComponentType component) -> Image {
    if (image.component() == component) return image;
    Image result(image.extents(), component, image.layout());
    with_type(image.component(), [&]<Component From>() {
        with_type(component,
                  [&]<Component To>() { recast<From, To>(image, result); });
    });
    return result;
}

} // namespace balsa::image
