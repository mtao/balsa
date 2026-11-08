#include "balsa/image/convert.hpp"

#include <zipper/Array.hpp>
#include <zipper/ArrayBase.hxx>
#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/as.hpp>
#include <zipper/expression/nullary/Zero.hpp>

#include <array>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace balsa::image {

namespace {
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

    // `source`'s pixels mapped from layout `from` to `to`.
    template <Component T>
    auto rearrange(const Pixels<T> &source, Channels from, Channels to)
        -> Pixels<T> {
        const auto A = channel_map(from, to, unit<T>);
        const auto mapped =
            ::zipper::as_array(source.template over<2>().template map<2>(
                [&](const auto &pixel) {
                    return A
                           * ::zipper::as_vector(pixel)
                                 .template cast<float>()
                                 .homogeneous();
                },
                ::zipper::dextents<1>(channel_count(to))));
        if constexpr (std::is_integral_v<T>) {
            return Pixels<T>(mapped.round().template cast<T>());
        } else {
            return Pixels<T>(mapped.template cast<T>());
        }
    }
} // namespace

auto convert_channels(const Image &image, Channels layout) -> Image {
    if (!image.layout()) {
        throw std::invalid_argument(
            "image::convert_channels: the image's layout is unknown");
    }
    if (image.layout() == layout) return image;
    return image.visit([&]<Component T>(const Pixels<T> &pixels) {
        return Image(rearrange(pixels, *image.layout(), layout), layout);
    });
}

auto convert_component(const Image &image, ComponentType component) -> Image {
    if (image.component() == component) return image;
    return image.visit([&]<Component From>(const Pixels<From> &pixels) {
        return with_component(component, [&]<Component To>() {
            return Image(
                Pixels<To>(from_unit<To>(pixels.template cast<float>() / unit<From>)),
                image.layout());
        });
    });
}

} // namespace balsa::image
