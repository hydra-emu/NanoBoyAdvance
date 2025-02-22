/*
 * Copyright (C) 2023 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <algorithm>
#include <fmt/format.h>
#include <nba/common/punning.hpp>

#include <QGridLayout>
#include <QGroupBox>
#include <QVBoxLayout>

#include "widget/debugger/utility.hpp"
#include "sprite_viewer.hpp"

// --------------------------------------------------------------------

static const auto CreateMonospaceLabel = [](QLabel*& label) {
  label = new QLabel{"-"};
  label->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
};

static const auto CreateCheckBox = [](QCheckBox*& check_box) {
  check_box = new QCheckBox{};
  check_box->setEnabled(false);
  return check_box;
};

// --------------------------------------------------------------------


SpriteViewer::SpriteViewer(nba::CoreBase* core, QWidget* parent) : QWidget{parent}, core{core} {
  // TODO(fleroviux): do lots of cleanup.

  pram = (u16 *) core->GetPRAM();
  vram = core->GetVRAM();
  oam = core->GetOAM();

  QHBoxLayout* hbox = new QHBoxLayout{};

  {
    QVBoxLayout* vbox = new QVBoxLayout{};

    // OAM # and sprite magnification
    {
      sprite_index_input = new QSpinBox{};
      sprite_index_input->setMinimum(0);
      sprite_index_input->setMaximum(127);

      magnification_input = new QSpinBox{};
      magnification_input->setMinimum(1);
      magnification_input->setMaximum(16);

      const auto grid = new QGridLayout{};
      int row = 0;
      grid->addWidget(new QLabel(tr("OAM #:")), row, 0);
      grid->addWidget(sprite_index_input, row++, 1);
      grid->addWidget(new QLabel(tr("Magnification:")), row, 0);
      grid->addWidget(magnification_input, row++, 1);
      vbox->addLayout(grid);
    }

    // Sprite attributes
    {
      QGroupBox *box = new QGroupBox{};
      box->setTitle("Object Attributes");
      QGridLayout *grid = new QGridLayout{};
      box->setMinimumWidth(235);

      int row = 0;

      const auto PushRow = [&](const QString &label, QWidget *widget) {
        grid->addWidget(new QLabel{QStringLiteral("%1:").arg(label)}, row, 0);
        grid->addWidget(widget, row++, 1);
      };

      PushRow("Enabled", CreateCheckBox(check_sprite_enabled));
      PushRow("Position", CreateMonospaceLabel(label_sprite_position));
      PushRow("Size", CreateMonospaceLabel(label_sprite_size));
      PushRow("Tile number", CreateMonospaceLabel(label_sprite_tile_number));
      PushRow("Palette", CreateMonospaceLabel(label_sprite_palette));
      PushRow("8BPP", CreateCheckBox(check_sprite_8bpp));
      PushRow("Flip V", CreateCheckBox(check_sprite_vflip));
      PushRow("Flip H", CreateCheckBox(check_sprite_hflip));
      PushRow("Mode", CreateMonospaceLabel(label_sprite_mode));
      PushRow("Affine", CreateCheckBox(check_sprite_affine));
      PushRow("Transform #", CreateMonospaceLabel(label_sprite_transform));
      PushRow("Double-size", CreateCheckBox(check_sprite_double_size));
      PushRow("Mosaic", CreateCheckBox(check_sprite_mosaic));
      PushRow("Render cycles", CreateMonospaceLabel(label_sprite_render_cycles));

      box->setLayout(grid);
      vbox->addWidget(box);
    }

    vbox->addStretch();
    hbox->addLayout(vbox);
  }

  {
    QVBoxLayout* vbox = new QVBoxLayout{};

    canvas = new QWidget{};
    canvas->setFixedSize(64, 64);
    canvas->setFixedSize(64, 64);
    canvas->installEventFilter(this);
    vbox->addWidget(canvas);
    vbox->addStretch(1);

    hbox->addLayout(vbox);
  }

  hbox->addStretch(1);

  setLayout(hbox);
}

void SpriteViewer::Update() {
  const int offset = sprite_index_input->value() << 3;

  const u16 attr0 = nba::read<u16>(oam, offset);
  const u16 attr1 = nba::read<u16>(oam, offset + 2);
  const u16 attr2 = nba::read<u16>(oam, offset + 4);

  const int shape = attr0 >> 14;
  const int size  = attr1 >> 14;

  static constexpr int k_sprite_size[4][4][2] = {
    { { 8 , 8  }, { 16, 16 }, { 32, 32 }, { 64, 64 } }, // Square
    { { 16, 8  }, { 32, 8  }, { 32, 16 }, { 64, 32 } }, // Horizontal
    { { 8 , 16 }, { 8 , 32 }, { 16, 32 }, { 32, 64 } }, // Vertical
    { { 8 , 8  }, { 8 , 8  }, { 8 , 8  }, { 8 , 8  } }  // Prohibited
  };

  const int width = k_sprite_size[shape][size][0];
  const int height = k_sprite_size[shape][size][1];

  const bool is_8bpp = attr0 & (1 << 13);
  const uint tile_number = attr2 & 0x3FFu;

  const bool one_dimensional_mapping = core->PeekHalfIO(0x04000000) & (1 << 6);

  const int tiles_x = width >> 3;
  const int tiles_y = height >> 3;

  u32* const buffer = (u32*)image_rgb32.bits();

  if(is_8bpp) {
    const u16* palette = &pram[256];

    for(int tile_y = 0; tile_y < tiles_y; tile_y++) {
      for(int tile_x = 0; tile_x < tiles_x; tile_x++) {
        int current_tile_number;

        if(one_dimensional_mapping) {
          current_tile_number = (tile_number + tile_y * (width >> 2) + (tile_x << 1)) & 0x3FF;
        } else {
          current_tile_number = ((tile_number + (tile_y * 32)) & 0x3E0) | (((tile_number & ~1) + (tile_x << 1)) & 0x1F);
        }

        // @todo: handle bad tile numbers and overflows and the likes

        u32 tile_address = 0x10000u + (current_tile_number << 5);

        for(int y = 0; y < 8; y++) {
          u64 tile_data = nba::read<u64>(vram, tile_address);

          u32* dst = &buffer[tile_y << 9 | y << 6 | tile_x << 3];

          for(int x = 0; x < 8; x++) {
            *dst++ = Rgb565ToArgb8888(palette[tile_data & 255u]);
            tile_data >>= 8;
          }

          tile_address += sizeof(u64);
        }
      }
    }
  } else {
    const u16* palette = &pram[256 | attr2 >> 12 << 4];

    for(int tile_y = 0; tile_y < tiles_y; tile_y++) {
      for(int tile_x = 0; tile_x < tiles_x; tile_x++) {
        int current_tile_number;

        if(one_dimensional_mapping) {
          current_tile_number = (tile_number + tile_y * (width >> 3) + tile_x) & 0x3FF;
        } else {
          current_tile_number = ((tile_number + (tile_y * 32)) & 0x3E0) | ((tile_number + tile_x) & 0x1F);
        }

        // @todo: handle bad tile numbers and overflows and the likes

        u32 tile_address = 0x10000u + (current_tile_number << 5);

        for(int y = 0; y < 8; y++) {
          u32 tile_data = nba::read<u32>(vram, tile_address);

          u32* dst = &buffer[tile_y << 9 | y << 6 | tile_x << 3];

          for(int x = 0; x < 8; x++) {
            *dst++ = Rgb565ToArgb8888(palette[tile_data & 15u]);
            tile_data >>= 4;
          }

          tile_address += sizeof(u32);
        }
      }
    }
  }

  static constexpr const char* k_mode_names[4] {
    "Normal", "Semi-Transparent", "Window", "Prohibited"
  };

  const uint x = attr1 & 0x1FFu;
  const uint y = attr0 & 0x0FFu;
  const bool affine = attr0 & (1 << 8);
  const uint palette = is_8bpp ? 0u : (attr2 >> 12);
  const uint mode = (attr0 >> 10) & 3;
  const bool mosaic = attr0 & (1 << 12);

  label_sprite_position->setText(QStringLiteral("%1, %2").arg(x).arg(y));
  label_sprite_size->setText(QStringLiteral("%1, %2").arg(width).arg(height));
  label_sprite_tile_number->setText(QStringLiteral("%1").arg(tile_number));
  label_sprite_palette->setText(QStringLiteral("%1").arg(palette));
  check_sprite_8bpp->setChecked(is_8bpp);
  label_sprite_mode->setText(k_mode_names[mode]);
  check_sprite_affine->setChecked(affine);
  check_sprite_mosaic->setChecked(mosaic);

  check_sprite_vflip->setEnabled(!affine);
  check_sprite_hflip->setEnabled(!affine);
  check_sprite_double_size->setEnabled(affine);

  const int signed_x = x >= 240 ? ((int)x - 512) : (int)x;

  int render_cycles = 0;

  if(affine) {
    const bool double_size = attr0 & (1 << 9);
    const uint transform = (attr1 >> 9) & 31u;

    check_sprite_enabled->setChecked(true);
    check_sprite_vflip->setChecked(false);
    check_sprite_hflip->setChecked(false);
    label_sprite_transform->setText(QStringLiteral("%1").arg(transform));
    check_sprite_double_size->setChecked(double_size);
    label_sprite_render_cycles->setText("0 (0%)");

    const int clipped_draw_width = std::max(0, (double_size ? 2 * width : width) + std::min(signed_x, 0));

    render_cycles = 10 + 2 * clipped_draw_width;
  } else {
    const bool enabled = !(attr0 & (1 << 9));
    const bool hflip = attr1 & (1 << 12);
    const bool vflip = attr1 & (1 << 13);

    check_sprite_enabled->setChecked(enabled);
    check_sprite_vflip->setChecked(vflip);
    check_sprite_hflip->setChecked(hflip);
    label_sprite_transform->setText("n/a");
    check_sprite_double_size->setChecked(false);
    label_sprite_render_cycles->setText("0 (0%)");

    if(enabled) {
      render_cycles = std::max(0, width + std::min(signed_x, 0));
    }
  }

  const int available_render_cycles = (core->PeekHalfIO(0x04000000) & (1 << 5)) ? 964 : 1232;

  label_sprite_render_cycles->setText(QString::fromStdString(fmt::format("{} ({:.2f} %)", render_cycles, 100.0f * (float)render_cycles / available_render_cycles)));

  sprite_width = width;
  sprite_height = height;

  const int magnification = magnification_input->value();
  magnified_sprite_width = width * magnification;
  magnified_sprite_height = height * magnification;
  canvas->setFixedSize(magnified_sprite_width, magnified_sprite_height);
  canvas->update();
}

bool SpriteViewer::eventFilter(QObject* object, QEvent* event) {
  if(object == canvas && event->type() == QEvent::Paint) {
    const QRect src_rect{0, 0, sprite_width, sprite_height};
    const QRect dst_rect{0, 0, magnified_sprite_width, magnified_sprite_height};

    QPainter painter{canvas};
    painter.drawImage(dst_rect, image_rgb32, src_rect);
    return true;
  }

  return false;
}