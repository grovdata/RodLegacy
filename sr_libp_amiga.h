#ifndef SR_LIBP_AMIGA_H_
#define SR_LIBP_AMIGA_H_

// This is a generated file! Please edit source .ksy file and use kaitai-struct-compiler to rebuild

#include "kaitai/kaitaistruct.h"
#include <stdint.h>
#include <vector>

#if KAITAI_STRUCT_VERSION < 9000L
#error "Incompatible Kaitai Struct C++/STL API: version 0.9 or later is required"
#endif

class sr_libp_amiga_t : public kaitai::kstruct {

public:
    class sprite_descriptor_t;

    sr_libp_amiga_t(kaitai::kstream* p__io, kaitai::kstruct* p__parent = 0, sr_libp_amiga_t* p__root = 0);

private:
    void _read();
    void _clean_up();

public:
    ~sr_libp_amiga_t();

    class sprite_descriptor_t : public kaitai::kstruct {

    public:

        sprite_descriptor_t(kaitai::kstream* p__io, sr_libp_amiga_t* p__parent = 0, sr_libp_amiga_t* p__root = 0);

    private:
        void _read();
        void _clean_up();

    public:
        ~sprite_descriptor_t();

    private:
        uint16_t m_width;
        uint16_t m_height;
        uint16_t m_size_decoded;
        uint8_t m_type;
        std::string m_directory;
        uint8_t m_zero;
        uint16_t m_offset;
        uint16_t m_size_encoded;
        sr_libp_amiga_t* m__root;
        sr_libp_amiga_t* m__parent;

    public:
        uint16_t width() const { return m_width; }
        uint16_t height() const { return m_height; }
        uint16_t size_decoded() const { return m_size_decoded; }

        /**
         * Varies from 0 up to 100+
         */
        uint8_t type() const { return m_type; }
        std::string directory() const { return m_directory; }

        /**
         * Enum 0-11
         */
        uint8_t zero() const { return m_zero; }
        uint16_t offset() const { return m_offset; }
        uint16_t size_encoded() const { return m_size_encoded; }
        sr_libp_amiga_t* _root() const { return m__root; }
        sr_libp_amiga_t* _parent() const { return m__parent; }
    };

private:
    uint16_t m_num_sprites;
    std::string m_crap;
    std::vector<sprite_descriptor_t*>* m_sprites;
    sr_libp_amiga_t* m__root;
    kaitai::kstruct* m__parent;

public:
    uint16_t num_sprites() const { return m_num_sprites; }
    std::string crap() const { return m_crap; }
    std::vector<sprite_descriptor_t*>* sprites() const { return m_sprites; }
    sr_libp_amiga_t* _root() const { return m__root; }
    kaitai::kstruct* _parent() const { return m__parent; }
};

#endif  // SR_LIBP_AMIGA_H_
