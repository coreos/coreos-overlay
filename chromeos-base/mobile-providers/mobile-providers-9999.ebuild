EAPI="2"
CROS_WORKON_PROJECT="chromiumos/third_party/mobile-broadband-provider-info"

inherit autotools cros-workon

DESCRIPTION="Database of mobile broadband service providers (with local modifications)"
HOMEPAGE="http://live.gnome.org/NetworkManager/MobileBroadband/ServiceProviders"

LICENSE="CC-PD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE="tools"

RDEPEND="!net-misc/mobile-broadband-provider-info
	>=dev-libs/glib-2.0"
DEPEND="${RDEPEND}
	>=dev-util/pkgconfig-0.9"

CROS_WORKON_LOCALNAME="../third_party/mobile-broadband-provider-info"

src_prepare() {
	eautoreconf
}

src_configure() {
	econf $(use_enable tools)
}

src_compile() {
	xmllint --valid --noout serviceproviders.xml || \
		die "XML document is not well-formed or is not valid"
	emake clean-generic || die "emake clean failed"
	emake || die "emake failed"
}

src_test() {
	emake check || die "building tests failed"
	if use x86 || use amd64 ; then
		gtester --verbose src/mobile_provider_unittest
	else
		echo "Skipping tests on non-x86 platform..."
	fi
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"
	dodoc NEWS README || die "dodoc failed"
}
