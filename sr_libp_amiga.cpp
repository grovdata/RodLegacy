// This is a generated file! Please edit source .ksy file and use kaitai-struct-compiler to rebuild

#include "sr_libp_amiga.h"

sr_libp_amiga_t::sr_libp_amiga_t(kaitai::kstream* p__io, kaitai::kstruct* p__parent, sr_libp_amiga_t* p__root) : kaitai::kstruct(p__io) {
    m__parent = p__parent;
    m__root = this;
    m_sprites = 0;

    try {
        _read();
    } catch(...) {
        _clean_up();
        throw;
    }
}

void sr_libp_amiga_t::_read() {
    m_num_sprites = m__io->read_u2be();
    m_crap = m__io->read_bytes((2 + (num_sprites() * 28)));
    m_sprites = new std::vector<sprite_descriptor_t*>();
    const int l_sprites = num_sprites();
    for (int i = 0; i < l_sprites; i++) {
        m_sprites->push_back(new sprite_descriptor_t(m__io, this, m__root));
    }
}

sr_libp_amiga_t::~sr_libp_amiga_t() {
    _clean_up();
}

void sr_libp_amiga_t::_clean_up() {
    if (m_sprites) {
        for (std::vector<sprite_descriptor_t*>::iterator it = m_sprites->begin(); it != m_sprites->end(); ++it) {
            delete *it;
        }
        delete m_sprites; m_sprites = 0;
    }
}

sr_libp_amiga_t::sprite_descriptor_t::sprite_descriptor_t(kaitai::kstream* p__io, sr_libp_amiga_t* p__parent, sr_libp_amiga_t* p__root) : kaitai::kstruct(p__io) {
    m__parent = p__parent;
    m__root = p__root;

    try {
        _read();
    } catch(...) {
        _clean_up();
        throw;
    }
}

void sr_libp_amiga_t::sprite_descriptor_t::_read() {
    m_width = m__io->read_u2be();
    m_height = m__io->read_u2be();
    m_size_decoded = m__io->read_u2be();
    m_type = m__io->read_u1();
    m_directory = m__io->read_bytes(16);
    m_zero = m__io->read_u1();
    m_offset = m__io->read_u2be();
    m_size_encoded = m__io->read_u2be();
}

sr_libp_amiga_t::sprite_descriptor_t::~sprite_descriptor_t() {
    _clean_up();
}

void sr_libp_amiga_t::sprite_descriptor_t::_clean_up() {
}
