#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Memory Pattern Master - PC display (แสดงผลอย่างเดียว ไม่ส่งคำสั่งเข้าบอร์ด)

อ่านข้อความที่เฟิร์มแวร์ส่งออกทาง UART (115200 8N1) แล้วแสดงเป็นหน้าจอเกม:
ระดับความยาก, รอบที่เล่น, HP, สีต้องห้าม, Reverse, นับถอยหลัง, เฉลยท้ายด่าน, สรุปผล

วิธีใช้
    pip install pyserial
    python memory_game_gui.py                 # หาพอร์ต ST-Link เองแล้วต่อให้
    python memory_game_gui.py --port COM12
    python memory_game_gui.py --demo          # ดูตัวอย่างโดยไม่ต้องมีบอร์ด (ไม่ต้องใช้ pyserial)

ข้อควรรู้: พอร์ต COM เปิดได้ทีละโปรแกรม ต้องปิด PuTTY / แท็บ serialterminal ก่อน
"""

import argparse
import collections
import itertools
import math
import queue
import random
import re
import sys
import threading
import time
import traceback

try:
    import tkinter as tk
    from tkinter import ttk, messagebox
except ImportError:  # pragma: no cover
    tk = None
    ttk = None
    messagebox = None

DEFAULT_BAUD = 115200
TOTAL_ROUNDS = 5          # ตรงกับ TOTAL_ROUNDS ใน game_types.h
MAX_HP = 3                # ตรงกับ HP_INIT ใน game_types.h

# ------------------------------------------------------------------ theme
BG = "#12141a"
PANEL = "#1b1f29"
CARD = "#262c3b"
CARD_DIM = "#202534"
TEXT = "#eef0f6"
MUTED = "#8d94a6"
GOOD = "#34c759"
BAD = "#ff453a"
WARN = "#ffb020"
GOLD = "#ffd60a"
ACCENT = "#5ac8fa"

# สีต้องตรงกับไฟจริงบนชิลด์ (ลำดับ LED 1-4 = PA5, PA6, PA7, PB6)
LED = collections.OrderedDict([
    ("BLUE", "#2f7bff"),
    ("RED", "#ff453a"),
    ("YELLOW", "#ffd60a"),
    ("GREEN", "#32d74b"),
])
DIFF_COLOR = {"EASY": GOOD, "MEDIUM": WARN, "HARD": BAD}

W, H = 900, 560           # ขนาดหน้าจอแบบ logical (คูณด้วย scale ตอนวาดจริง)


# ================================================================== parsing
# Welcome / Turn the dial ใช้ search (ไม่ยึดต้นบรรทัด) เพราะถ้ากดรีเซ็ตกลางข้อความ
# ข้อความที่ส่งไม่จบจะไปต่อหน้า Welcome บนบรรทัดเดียวกัน เช่น "Don't preWelcome to ..."
_RX_WELCOME = re.compile(r"Welcome to Memory Pattern", re.I)
_RX_TURN = re.compile(r"Turn the dial to set", re.I)
_RX_DIFF = re.compile(r"^Difficulty:\s*(EASY|MEDIUM|HARD)\b", re.I)
_RX_ROUND = re.compile(r"^Round\s+(\d+)\s*$", re.I)
_RX_DONT = re.compile(r"^Don.?t press:\s*([A-Za-z]+)", re.I)
_RX_REVERSE = re.compile(r"^Reverse:\s*(YES|NO)\b", re.I)
_RX_COUNT = re.compile(r"^([123])$")
_RX_WRONG = re.compile(r"^You got\s+(\d+)\s+wrong\b", re.I)
_RX_ANSWER = re.compile(r"^Answer:\s*(.*)$", re.I)
# รองรับทั้ง "HP -1. You have 2 left." / "...2 HP left." / "HP -1. HP left: 2"
_RX_HP = re.compile(r"^HP\s*-\s*1\D+?(\d+)", re.I)
_RX_END = re.compile(r"^End\s*$", re.I)
_RX_PERFECT = re.compile(r"^Perfect!?\s*$", re.I)
_RX_CONGRATS = re.compile(r"^Congratulations!?\s*$", re.I)
_RX_OUTOF = re.compile(
    r"^Out of\s+(\d+)\s+rounds?,\s*you got\s+(\d+)\s+wrong\.?\s*"
    r"Your accuracy is\s+(\d+)\s*%", re.I)
_RX_ALLRIGHT = re.compile(r"^You got everything correct.*accuracy is\s+(\d+)\s*%", re.I)
_RX_GAMEOVER = re.compile(r"^GAME OVER\b", re.I)


def clean(raw):
    """ตัดอักขระควบคุม/ขยะบนสายออก (เช่น ไบต์แปลกๆ ตอนบอร์ดบูต) เหลือ ASCII ที่พิมพ์ได้"""
    return re.sub(r"[^\x20-\x7e]", "", raw).strip()


class Game(object):
    """สถานะเกมที่ประกอบขึ้นจากข้อความที่อ่านได้ (ไม่รู้จัก Tk หรือพอร์ตเลย ทดสอบแยกได้)"""

    def __init__(self):
        self.version = 0
        self.last_result = None        # (ข้อความ, สี) ของเกมก่อนหน้า
        self._new_game()
        self.phase = "waiting"
        self.hp = None                 # ยังไม่รู้จนกว่าจะเห็น Welcome หรือบรรทัด HP

    def _new_game(self):
        self.phase = "welcome"
        self.difficulty = None
        self.round = 0
        self.total_rounds = TOTAL_ROUNDS
        self.hp = MAX_HP
        self.round_results = {}        # รอบ -> จำนวนครั้งที่กดผิด
        self.summary = None
        self.final_done = False
        self._new_round_fields()

    def _new_round_fields(self):
        self.forbidden = None          # None = ยังไม่รู้, "NONE" = ไม่มีสีต้องห้าม
        self.reverse = None            # None = ยังไม่รู้
        self.countdown = None
        self.countdown_ts = None
        self.wrong = None
        self.answer = None             # None = ยังไม่มา, [] = ไม่มีอะไรต้องกด
        self.hp_lost = False

    # ---------------------------------------------------------------- feed
    def feed(self, line, now):
        """รับ 1 บรรทัด คืนชื่อเหตุการณ์ที่รู้จัก (หรือ None ถ้าไม่รู้จัก)"""
        text = clean(line)
        if not text:
            return None
        ev = self._dispatch(text, now)
        if ev is not None:
            self.version += 1
        return ev

    def _dispatch(self, text, now):
        if _RX_WELCOME.search(text):
            self._new_game()
            return "welcome"
        if _RX_TURN.search(text):
            self.phase = "select"
            return "select"
        m = _RX_DIFF.match(text)
        if m:
            self.difficulty = m.group(1).upper()
            self.phase = "difficulty"
            return "difficulty"
        m = _RX_ROUND.match(text)
        if m:
            n = int(m.group(1))
            self.round = n
            self.round_results = dict((k, v) for k, v in self.round_results.items() if k < n)
            self.summary = None
            self.final_done = False
            self._new_round_fields()
            self.phase = "announce"
            return "round"
        m = _RX_DONT.match(text)
        if m:
            name = m.group(1).upper()
            self.forbidden = name if name in LED else "NONE"
            return "dont_press"
        m = _RX_REVERSE.match(text)
        if m:
            self.reverse = (m.group(1).upper() == "YES")
            return "reverse"
        m = _RX_COUNT.match(text)
        if m:
            self.countdown = int(m.group(1))
            self.countdown_ts = now
            self.phase = "countdown"
            return "countdown"
        m = _RX_WRONG.match(text)
        if m:
            self.wrong = int(m.group(1))
            self.round_results[self.round] = self.wrong
            self.answer = None
            self.hp_lost = False
            self.phase = "result"
            return "wrong"
        m = _RX_ANSWER.match(text)
        if m:
            self.answer = [t.upper() for t in m.group(1).split() if t.upper() in LED]
            if self.phase != "result":
                self.phase = "result"
            return "answer"
        m = _RX_HP.match(text)
        if m:
            self.hp = int(m.group(1))
            self.hp_lost = True
            return "hp"
        if _RX_END.match(text):
            self.final_done = True
            return "end"
        if _RX_PERFECT.match(text):
            self.summary = {"kind": "perfect", "rounds": None, "wrong": None, "accuracy": None}
            self.phase = "summary"
            return "perfect"
        if _RX_CONGRATS.match(text):
            self.summary = {"kind": "congrats", "rounds": None, "wrong": None, "accuracy": None}
            self.phase = "summary"
            return "congrats"
        m = _RX_OUTOF.match(text)
        if m:
            rounds, wrong, acc = int(m.group(1)), int(m.group(2)), int(m.group(3))
            if self.summary is None:
                self.summary = {"kind": "perfect" if wrong == 0 else "congrats"}
            self.summary.update(rounds=rounds, wrong=wrong, accuracy=acc)
            self.total_rounds = rounds
            self.phase = "summary"
            self._remember_result()
            return "summary"
        m = _RX_ALLRIGHT.match(text)
        if m:
            acc = int(m.group(1))
            self.summary = {"kind": "perfect", "rounds": self.total_rounds,
                            "wrong": 0, "accuracy": acc}
            self.phase = "summary"
            self._remember_result()
            return "summary"
        if _RX_GAMEOVER.match(text):
            self.hp = 0
            self.phase = "gameover"
            self.last_result = ("GAME OVER (reached round %d)" % self.round, BAD)
            return "gameover"
        return None

    def _remember_result(self):
        s = self.summary or {}
        label = "Perfect!" if s.get("kind") == "perfect" else "Congratulations!"
        self.last_result = ("%s %s%%" % (label, s.get("accuracy")),
                            GOLD if s.get("kind") == "perfect" else GOOD)

    # ---------------------------------------------------------------- view
    def view_phase(self, now):
        """หน้าที่ควรโชว์ ณ เวลานี้ (หลังเลข 1 ผ่านไป 1 วิ ไฟ LED เริ่มเล่นแพทเทิร์น)"""
        if (self.phase == "countdown" and self.countdown == 1
                and self.countdown_ts is not None and now - self.countdown_ts >= 1.0):
            return "playing"
        return self.phase


# ================================================================== painting
class Painter(object):
    """ตัวช่วยวาดบน canvas โดยรับพิกัดแบบ logical แล้วคูณ scale ให้ (ใช้ได้กับ Tk และตัวจำลองใน test)"""

    def __init__(self, canvas, scale=1.0, family="Segoe UI"):
        self.c = canvas
        self.s = float(scale)
        self.family = family

    def font(self, px, bold=False):
        # ขนาดติดลบ = หน่วยพิกเซล จะได้สเกลไปพร้อมกับพิกัดเสมอ ไม่ขึ้นกับ tk scaling
        return (self.family, -max(1, int(round(px * self.s))), "bold" if bold else "normal")

    def rect(self, x1, y1, x2, y2, fill="", outline="", width=1):
        s = self.s
        return self.c.create_rectangle(x1 * s, y1 * s, x2 * s, y2 * s, fill=fill,
                                       outline=outline, width=width)

    def rrect(self, x1, y1, x2, y2, r, fill="", outline="", width=1):
        s = self.s
        r = min(r, (x2 - x1) / 2.0, (y2 - y1) / 2.0)
        pts = []
        corners = [(x2 - r, y1 + r, -90), (x2 - r, y2 - r, 0), (x1 + r, y2 - r, 90), (x1 + r, y1 + r, 180)]
        for cx, cy, a0 in corners:
            for i in range(0, 10):
                a = math.radians(a0 + i * 10)
                pts.append((cx + r * math.cos(a)) * s)
                pts.append((cy + r * math.sin(a)) * s)
        return self.c.create_polygon(pts, fill=fill, outline=outline, width=width)

    def oval(self, cx, cy, r, fill="", outline="", width=1):
        s = self.s
        return self.c.create_oval((cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s,
                                  fill=fill, outline=outline, width=width)

    def heart(self, cx, cy, half_w, fill="", outline="", width=2):
        s = self.s
        k = half_w / 16.0
        pts = []
        for i in range(40):
            t = 2 * math.pi * i / 40
            x = 16 * math.sin(t) ** 3
            y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
            pts.append((cx + x * k) * s)
            pts.append((cy - (y + 2.5) * k) * s)
        return self.c.create_polygon(pts, fill=fill, outline=outline, width=width)

    def text(self, x, y, s, px, color=TEXT, bold=False, anchor="center", width=None):
        kw = dict(text=s, fill=color, font=self.font(px, bold), anchor=anchor)
        if width:
            kw["width"] = int(width * self.s)
            kw["justify"] = "center" if anchor == "center" else "left"
        return self.c.create_text(x * self.s, y * self.s, **kw)


def _header(p, g):
    y1, y2 = 16, 100
    # --- difficulty
    p.rrect(24, y1, 250, y2, 16, fill=PANEL)
    p.text(42, y1 + 24, "DIFFICULTY", 13, MUTED, bold=True, anchor="w")
    if g.difficulty:
        p.text(42, y1 + 58, g.difficulty, 30, DIFF_COLOR.get(g.difficulty, TEXT), bold=True, anchor="w")
    else:
        p.text(42, y1 + 58, "\u2014", 30, MUTED, bold=True, anchor="w")
    # --- rounds
    p.rrect(266, y1, 634, y2, 16, fill=PANEL)
    label = "ROUND" if not g.round else "ROUND %d OF %d" % (g.round, g.total_rounds)
    p.text(450, y1 + 24, label, 13, MUTED, bold=True)
    n = max(1, g.total_rounds)
    gap = min(62, 330 // n)
    for i in range(1, n + 1):
        cx = 450 + (i - (n + 1) / 2.0) * gap
        cy = y1 + 58
        done = g.round_results.get(i)
        if done is not None:
            p.oval(cx, cy, 18, fill=GOOD if done == 0 else BAD)
            p.text(cx, cy, str(i), 16, "#0b0d12", bold=True)
        elif i == g.round and g.phase in ("announce", "countdown", "difficulty"):
            p.oval(cx, cy, 18, fill=CARD, outline=TEXT, width=3)
            p.text(cx, cy, str(i), 16, TEXT, bold=True)
        else:
            p.oval(cx, cy, 18, fill=CARD_DIM)
            p.text(cx, cy, str(i), 16, MUTED, bold=True)
    # --- hp
    p.rrect(650, y1, 876, y2, 16, fill=PANEL)
    p.text(668, y1 + 24, "HP", 13, MUTED, bold=True, anchor="w")
    for i in range(MAX_HP):
        cx = 705 + i * 62
        cy = y1 + 56
        if g.hp is None:
            p.heart(cx, cy, 20, fill="", outline=MUTED, width=2)
        elif i < g.hp:
            p.heart(cx, cy, 20, fill=BAD, outline=BAD, width=2)
        else:
            p.heart(cx, cy, 20, fill="", outline=MUTED, width=2)


def _reminders(p, g, y):
    """การ์ด Don't press / Reverse ไว้เตือนผู้เล่นระหว่างรอ/นับถอยหลัง/ดูไฟ"""
    h = 92
    x1, x2 = 135, 435
    p.rrect(x1, y, x2, y + h, 16, fill=CARD)
    p.text(x1 + 22, y + 24, "DON'T PRESS", 13, MUTED, bold=True, anchor="w")
    f = g.forbidden
    if f is None:
        p.text(x1 + 22, y + 60, "\u2026", 32, MUTED, bold=True, anchor="w")
    elif f in LED:
        p.oval(x1 + 38, y + 60, 15, fill=LED[f])
        p.text(x1 + 64, y + 60, f, 32, LED[f], bold=True, anchor="w")
    else:
        p.text(x1 + 22, y + 60, "None", 32, TEXT, bold=True, anchor="w")

    x1, x2 = 465, 765
    p.rrect(x1, y, x2, y + h, 16, fill=CARD)
    p.text(x1 + 22, y + 24, "REVERSE", 13, MUTED, bold=True, anchor="w")
    if g.reverse is None:
        p.text(x1 + 22, y + 60, "\u2026", 32, MUTED, bold=True, anchor="w")
    elif g.reverse:
        p.text(x1 + 22, y + 60, "YES", 32, WARN, bold=True, anchor="w")
        p.text(x2 - 22, y + 62, "press in reverse order", 15, MUTED, anchor="e")
    else:
        p.text(x1 + 22, y + 60, "NO", 32, TEXT, bold=True, anchor="w")


def _last_result(p, g, y):
    if g.last_result:
        text, color = g.last_result
        p.text(450, y, "Last game: " + text, 20, color, bold=True)


def _view_waiting(p, g, conn):
    if conn == "disconnected":
        title, sub, sub2 = ("Not connected",
                            "Pick the board's COM port above, then press Connect",
                            "Close PuTTY / other serial programs first")
    elif conn == "connecting":
        title, sub, sub2 = "Connecting\u2026", "", ""
    else:
        title, sub, sub2 = ("Waiting for the board\u2026",
                            "Press the board's reset button, or start playing",
                            "The screen follows whatever the board prints")
    p.text(450, 290, title, 44, TEXT, bold=True)
    if sub:
        p.text(450, 352, sub, 22, MUTED, width=760)
    if sub2:
        p.text(450, 392, sub2, 18, MUTED, width=760)


def _view_welcome(p, g, conn):
    p.text(450, 215, "Welcome to", 30, MUTED)
    p.text(450, 285, "Memory Pattern Master Game!", 40, TEXT, bold=True, width=820)
    p.text(450, 365, "Get ready\u2026", 22, MUTED)
    _last_result(p, g, 470)


def _view_select(p, g, conn):
    p.text(450, 175, "Choose your difficulty", 44, TEXT, bold=True)
    chips = [("EASY", "turn the dial left"), ("MEDIUM", "centre"), ("HARD", "turn the dial right")]
    for i, (name, cap) in enumerate(chips):
        x1 = 120 + i * 230
        p.rrect(x1, 235, x1 + 200, 305, 16, fill=CARD)
        p.text(x1 + 100, 270, name, 28, DIFF_COLOR[name], bold=True)
        p.text(x1 + 100, 333, cap, 16, MUTED)
    p.text(450, 405, "then press BUTTON 1 (the first button from the left)", 23, TEXT, width=800)
    _last_result(p, g, 480)


def _view_difficulty(p, g, conn):
    p.text(450, 205, "Difficulty set", 30, MUTED)
    p.text(450, 300, g.difficulty or "", 100, DIFF_COLOR.get(g.difficulty, TEXT), bold=True)
    p.text(450, 405, "Starting Round 1\u2026", 22, MUTED)


def _view_announce(p, g, conn):
    p.text(450, 200, "ROUND %d" % g.round, 72, TEXT, bold=True)
    p.text(450, 268, "Get ready\u2026", 22, MUTED)
    _reminders(p, g, 335)


def _view_countdown(p, g, conn):
    p.text(450, 245, str(g.countdown), 150, ACCENT, bold=True)
    _reminders(p, g, 390)


def _view_playing(p, g, conn):
    p.text(450, 190, "WATCH THE LEDs", 56, ACCENT, bold=True)
    p.text(450, 248, "then repeat the pattern on the buttons", 22, MUTED)
    for i, name in enumerate(LED):
        p.oval(450 + (i - 1.5) * 84, 318, 22, fill=LED[name])
    _reminders(p, g, 392)


def _view_result(p, g, conn):
    wrong = g.wrong
    if wrong is None:
        p.text(450, 300, "\u2026", 60, MUTED, bold=True)
        return
    p.text(450, 175, "You got %d wrong." % wrong, 54, GOOD if wrong == 0 else BAD, bold=True)
    if g.hp_lost:
        p.text(450, 225, "HP \u22121  \u2022  %d left" % g.hp, 22, BAD, bold=True)
    elif wrong == 0:
        p.text(450, 225, "No mistakes this round", 22, GOOD)
    p.text(450, 290, "CORRECT ANSWER", 14, MUTED, bold=True)
    ans = g.answer
    if ans is None:
        p.text(450, 350, "\u2026", 40, MUTED, bold=True)
    elif not ans:
        p.text(450, 350, "Nothing to press this round", 24, MUTED)
    else:
        n = len(ans)
        gap = min(70, 760 // n)
        for i, name in enumerate(ans):
            cx = 450 + (i - (n - 1) / 2.0) * gap
            p.oval(cx, 350, 26, fill=LED[name])
            p.text(cx, 396, str(i + 1), 14, MUTED)
    notes = []
    if g.reverse:
        notes.append("reversed order")
    if g.forbidden in LED:
        notes.append("%s skipped" % g.forbidden)
    if notes:
        p.text(450, 438, "  \u2022  ".join(notes), 18, MUTED)
    if g.final_done:
        p.text(450, 492, "Final round complete", 20, TEXT, bold=True)


def _view_summary(p, g, conn):
    s = g.summary or {}
    perfect = s.get("kind") == "perfect"
    p.text(450, 180, "Perfect!" if perfect else "Congratulations!", 62,
           GOLD if perfect else GOOD, bold=True)
    rounds, wrong, acc = s.get("rounds"), s.get("wrong"), s.get("accuracy")
    if rounds is not None:
        if wrong == 0:
            line = "You got everything right in all %d rounds" % rounds
        else:
            line = "Out of %d rounds, you got %d wrong" % (rounds, wrong)
        p.text(450, 245, line, 24, TEXT)
    if acc is not None:
        p.text(450, 338, "%d%%" % acc, 104, ACCENT, bold=True)
        p.text(450, 432, "ACCURACY", 16, MUTED, bold=True)
    if g.hp is not None:
        p.text(450, 478, "HP left: %d" % g.hp, 20, MUTED)


def _view_gameover(p, g, conn):
    p.text(450, 215, "GAME OVER", 84, BAD, bold=True)
    p.text(450, 295, "You lost all HP", 30, TEXT)
    if g.round:
        p.text(450, 345, "Reached round %d of %d" % (g.round, g.total_rounds), 22, MUTED)
    p.text(450, 420, "Starting a new game soon\u2026", 18, MUTED)


_VIEWS = {
    "waiting": _view_waiting,
    "welcome": _view_welcome,
    "select": _view_select,
    "difficulty": _view_difficulty,
    "announce": _view_announce,
    "countdown": _view_countdown,
    "playing": _view_playing,
    "result": _view_result,
    "summary": _view_summary,
    "gameover": _view_gameover,
}


def render(p, g, now, conn):
    """วาดทั้งหน้าจอ: แถบบน (ความยาก/รอบ/HP) + แผงกลางตามหน้าปัจจุบัน"""
    p.rect(0, 0, W, H, fill=BG)
    _header(p, g)
    p.rrect(24, 116, 876, 536, 22, fill=PANEL)
    view = g.view_phase(now)
    _VIEWS.get(view, _view_waiting)(p, g, conn)


# ================================================================== sources
def _load_serial():
    """โหลด pyserial เมื่อต้องใช้จริง (โหมด --demo ไม่ต้องมี)"""
    try:
        import serial  # noqa
        import serial.tools.list_ports  # noqa
        return serial
    except ImportError:
        return None


def _list_ports():
    """คืนรายการ (device, คำอธิบาย, เป็นพอร์ตของ ST-Link หรือไม่)"""
    ser = _load_serial()
    if ser is None:
        return []
    out = []
    for info in ser.tools.list_ports.comports():
        desc = info.description or ""
        manu = getattr(info, "manufacturer", "") or ""
        is_st = (getattr(info, "vid", None) == 0x0483
                 or "stlink" in desc.lower().replace(" ", "")
                 or "stmicro" in desc.lower() or "stmicro" in manu.lower())
        out.append((info.device, desc, is_st))
    out.sort(key=lambda t: (not t[2], t[0]))
    return out


class SerialReader(threading.Thread):
    """อ่านพอร์ตใน thread แยก แยกเป็นบรรทัดแล้วโยนเข้า queue (GUI ไม่แตะพอร์ตโดยตรง)

    ทุกข้อความใน queue เป็น (kind, token, payload)
    kind: opened / line / open_failed / lost / closed
    """

    def __init__(self, serial_mod, port, baud, out_q, token):
        threading.Thread.__init__(self)
        self.daemon = True
        self.serial_mod = serial_mod
        self.port = port
        self.baud = baud
        self.out_q = out_q
        self.token = token
        self.stop_event = threading.Event()   # ห้ามตั้งชื่อ _stop (ชนกับของ Thread)

    def stop(self):
        self.stop_event.set()

    def _put(self, kind, payload=None):
        self.out_q.put((kind, self.token, payload))

    def run(self):
        try:
            ser = self.serial_mod.Serial(self.port, self.baud, timeout=0.1)
        except Exception as e:  # พอร์ตถูกใช้อยู่ / ไม่มีพอร์ต
            self._put("open_failed", "%s: %s" % (type(e).__name__, e))
            return
        self._put("opened", self.port)
        buf = bytearray()
        reason = None
        try:
            while not self.stop_event.is_set():
                try:
                    chunk = ser.read(ser.in_waiting or 1)
                except Exception as e:      # ถอดสาย USB ระหว่างใช้งาน
                    reason = "%s: %s" % (type(e).__name__, e)
                    break
                if not chunk:
                    continue
                buf.extend(chunk)
                while True:
                    i = buf.find(b"\n")
                    if i < 0:
                        break
                    raw = bytes(buf[:i])
                    del buf[:i + 1]
                    self._put("line", raw.decode("utf-8", "replace"))
                if len(buf) > 4096:         # ขยะยาวๆ ที่ไม่มีขึ้นบรรทัดใหม่ ทิ้งกันบวม
                    del buf[:]
        finally:
            try:
                ser.close()
            except Exception:
                pass
        if reason is not None and not self.stop_event.is_set():
            self._put("lost", reason)
        else:
            self._put("closed")


# ---------------------------------------------------------------- demo
PATTERN_LEN = {"EASY": 4, "MEDIUM": 6, "HARD": 8}

# แต่ละรอบ = (สีต้องห้าม หรือ None, reverse, จำนวนครั้งที่กดผิด)
DEMO_SCENARIOS = [
    ("EASY", [(None, False, 0), (None, False, 0), ("RED", False, 0), (None, True, 0), ("BLUE", True, 0)]),
    ("MEDIUM", [(None, False, 0), (None, True, 3), ("GREEN", False, 2), (None, False, 0), ("RED", True, 0)]),
    ("HARD", [("YELLOW", False, 2), (None, True, 3), (None, False, 1)]),
]


def make_script(difficulty, rounds, seed=1):
    """สร้างลำดับข้อความ (หน่วงก่อนส่ง, ข้อความ) ให้เหมือนที่เฟิร์มแวร์ส่งจริง รวมจังหวะเวลา"""
    rng = random.Random(seed)
    names = list(LED)
    out = []

    def add(delay, text):
        out.append((delay, text))

    add(0.0, "Welcome to Memory Pattern Master Game!")
    add(0.0, "Turn the dial to set difficulty, then press BUTTON 1 "
             "(the first button from the left) to confirm.")
    add(5.0, "Difficulty: %s" % difficulty)
    hp = MAX_HP
    total = len(rounds)
    dead = False
    for i, (forb, rev, wrong) in enumerate(rounds, 1):
        add(5.0 if i > 1 else 0.0, "Round %d" % i)
        add(1.0, "Don't press: %s" % (forb or "NONE"))
        add(0.0, "Reverse: %s" % ("YES" if rev else "NO"))
        add(3.0, "3")
        add(1.0, "2")
        add(1.0, "1")
        length = PATTERN_LEN[difficulty]
        pattern = [rng.choice(names) for _ in range(length)]
        expected = list(reversed(pattern)) if rev else list(pattern)
        expected = [c for c in expected if c != forb]
        add(1.0 + 0.6 * length + 4.0, "You got %d wrong." % wrong)
        add(0.0, "Answer: " + " ".join(expected))
        if wrong:
            hp -= 1
            add(0.0, "HP -1. You have %d HP left." % hp)
        if hp <= 0:
            add(0.0, "GAME OVER")
            dead = True
            break
    if not dead:
        wrong_rounds = sum(1 for r in rounds if r[2] > 0)
        add(0.0, "End")
        add(0.0, "Perfect!" if wrong_rounds == 0 else "Congratulations!")
        add(0.0, "Out of %d rounds, you got %d wrong. Your accuracy is %d%%."
            % (total, wrong_rounds, (total - wrong_rounds) * 100 // total))
    return out


class DemoSource(threading.Thread):
    """ส่งข้อความตัวอย่างเข้า queue เหมือนบอร์ดจริง (วนเล่นหลายสถานการณ์ไปเรื่อยๆ)"""

    def __init__(self, out_q, token, speed=2.0, gap=5.0):
        threading.Thread.__init__(self)
        self.daemon = True
        self.out_q = out_q
        self.token = token
        self.speed = max(0.1, float(speed))
        self.gap = gap
        self.stop_event = threading.Event()

    def stop(self):
        self.stop_event.set()

    def _sleep(self, seconds):
        end = time.monotonic() + seconds / self.speed
        while not self.stop_event.is_set():
            left = end - time.monotonic()
            if left <= 0:
                return
            time.sleep(min(0.05, left))

    def run(self):
        self.out_q.put(("opened", self.token, "demo"))
        for n, (diff, rounds) in enumerate(itertools.cycle(DEMO_SCENARIOS)):
            for delay, text in make_script(diff, rounds, seed=n + 1):
                self._sleep(delay)
                if self.stop_event.is_set():
                    self.out_q.put(("closed", self.token, None))
                    return
                self.out_q.put(("line", self.token, text))
            self._sleep(self.gap)
            if self.stop_event.is_set():
                break
        self.out_q.put(("closed", self.token, None))


# ================================================================== app
class App(object):
    TICK_MS = 40
    MAX_LOG_LINES = 400

    def __init__(self, root, args, clock=None):
        self.root = root
        self.args = args
        self.clock = clock or time.monotonic
        self.q = queue.Queue()
        self.game = Game()
        self.source = None
        self.token = 0
        self.speed = 1.0
        self.conn = "disconnected"     # disconnected / connecting / connected / demo
        self.port_map = {}
        self.last_key = None
        self._log_lines = 0
        self._build_ui()
        self._refresh_ports()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(self.TICK_MS, self._tick)
        self._autostart()

    # ------------------------------------------------------------ ui
    def _build_ui(self):
        r = self.root
        r.title("Memory Pattern Master - PC Display")
        r.configure(bg=BG)
        r.resizable(False, False)
        try:
            self.S = max(1.0, min(3.0, r.winfo_fpixels("1i") / 96.0))
        except Exception:
            self.S = 1.0

        bar = tk.Frame(r, bg=BG)
        bar.pack(fill="x", padx=12, pady=(10, 6))
        tk.Label(bar, text="Port", bg=BG, fg=MUTED).pack(side="left")
        self.port_var = tk.StringVar()
        self.port_box = ttk.Combobox(bar, textvariable=self.port_var, state="readonly", width=46)
        self.port_box.pack(side="left", padx=(6, 6))
        tk.Button(bar, text="Refresh", command=self._refresh_ports,
                  bg=CARD, fg=TEXT, activebackground=CARD_DIM, activeforeground=TEXT,
                  relief="flat", padx=10).pack(side="left", padx=(0, 6))
        self.btn_connect = tk.Button(bar, text="Connect", command=self.toggle_connection,
                                     bg=ACCENT, fg="#0b0d12", activebackground=ACCENT,
                                     activeforeground="#0b0d12", relief="flat", padx=14)
        self.btn_connect.pack(side="left")
        self.status = tk.Label(bar, text="Not connected", bg=BG, fg=MUTED, anchor="w")
        self.status.pack(side="left", padx=(14, 0))

        self.canvas = tk.Canvas(r, width=int(W * self.S), height=int(H * self.S),
                                bg=BG, highlightthickness=0)
        self.canvas.pack(padx=12)

        self.show_log = tk.BooleanVar(value=False)
        tk.Checkbutton(r, text="Show raw log", variable=self.show_log, command=self._toggle_log,
                       bg=BG, fg=MUTED, selectcolor=BG, activebackground=BG,
                       activeforeground=TEXT, anchor="w").pack(fill="x", padx=12, pady=(4, 6))
        self.log_frame = tk.Frame(r, bg=BG)
        self.log = tk.Text(self.log_frame, height=8, state="disabled", bg=PANEL, fg=TEXT,
                           wrap="none", relief="flat", borderwidth=0, highlightthickness=0)
        self.log.pack(fill="x")

    def _toggle_log(self):
        if self.show_log.get():
            self.log_frame.pack(fill="x", padx=12, pady=(0, 12))
        else:
            self.log_frame.pack_forget()

    def _append_log(self, text):
        self.log.config(state="normal")
        self.log.insert("end", text + "\n")
        self._log_lines += 1
        if self._log_lines > self.MAX_LOG_LINES:
            self.log.delete("1.0", "2.0")
            self._log_lines -= 1
        self.log.see("end")
        self.log.config(state="disabled")

    def _set_status(self, text, kind="idle"):
        color = {"ok": GOOD, "error": BAD}.get(kind, MUTED)
        self.status.config(text=text, fg=color)

    def _update_button(self):
        if self.source is not None:
            self.btn_connect.config(text="Stop demo" if self.conn == "demo" else "Disconnect")
        else:
            self.btn_connect.config(text="Connect")

    # ------------------------------------------------------------ ports
    def _refresh_ports(self):
        ports = _list_ports()
        labels = []
        self.port_map = {}
        for dev, desc, is_st in ports:
            label = "%s  -  %s" % (dev, desc) if desc else dev
            labels.append(label)
            self.port_map[label] = dev
        self.port_box["values"] = labels
        if labels:
            current = self.port_var.get()
            if current not in self.port_map:
                self.port_var.set(labels[0])      # ST-Link ถูกเรียงขึ้นมาก่อนแล้ว
        else:
            self.port_var.set("")
        return ports

    def _selected_device(self):
        label = self.port_var.get()
        if label in self.port_map:
            return self.port_map[label]
        return label.split()[0] if label else None

    def _autostart(self):
        a = self.args
        if a.demo:
            self.start_demo(a.speed)
        elif a.port:
            self.start_serial(a.port)
        elif not a.no_auto:
            ports = _list_ports()
            if ports and ports[0][2]:
                self.start_serial(ports[0][0])
            elif _load_serial() is None:
                self._set_status("pyserial not installed: pip install pyserial  (or run with --demo)", "error")

    # ------------------------------------------------------------ connect
    def toggle_connection(self):
        if self.source is not None:
            self.disconnect()
        else:
            dev = self._selected_device()
            if not dev:
                self._set_status("No serial port selected (plug in the board, then Refresh)", "error")
                return
            self.start_serial(dev)

    def _stop_source(self):
        src = self.source
        self.source = None
        if src is not None:
            src.stop()
            src.join(1.0)
        self.token += 1                      # ข้อความค้างจากตัวเก่าจะถูกข้าม

    def disconnect(self):
        self._stop_source()
        self.conn = "disconnected"
        self.game = Game()
        self._set_status("Disconnected", "idle")
        self._update_button()

    def start_serial(self, device):
        ser = _load_serial()
        if ser is None:
            self._set_status("pyserial not installed: pip install pyserial  (or run with --demo)", "error")
            return
        self._stop_source()
        self.token += 1
        self.speed = 1.0
        self.game = Game()
        self.conn = "connecting"
        self.source = SerialReader(ser, device, self.args.baud, self.q, self.token)
        self.source.start()
        self._set_status("Connecting to %s \u2026" % device, "idle")
        self._update_button()

    def start_demo(self, speed):
        self._stop_source()
        self.token += 1
        self.speed = max(0.1, float(speed))
        self.game = Game()
        self.conn = "demo"
        self.source = DemoSource(self.q, self.token, speed=self.speed)
        self.source.start()
        self._set_status("Demo mode (%.1fx) - no board needed" % self.speed, "ok")
        self._update_button()

    def game_now(self):
        return self.clock() * self.speed

    # ------------------------------------------------------------ loop
    def _tick(self):
        try:
            self._drain()
            self._redraw_if_needed()
        except Exception:
            traceback.print_exc()
        self.root.after(self.TICK_MS, self._tick)

    def _drain(self):
        for _ in range(300):
            try:
                kind, token, payload = self.q.get_nowait()
            except queue.Empty:
                break
            if token != self.token:
                continue
            if kind == "line":
                text = clean(payload)
                if text:
                    self._append_log(text)
                self.game.feed(payload, self.game_now())
            elif kind == "opened":
                if self.conn != "demo":
                    self.conn = "connected"
                    self._set_status("Connected to %s @ %d" % (payload, self.args.baud), "ok")
            elif kind == "open_failed":
                self.source = None
                self.conn = "disconnected"
                self._set_status(self._explain_open_error(payload), "error")
                self._update_button()
            elif kind == "lost":
                self.source = None
                self.conn = "disconnected"
                self._set_status("Connection lost (USB unplugged?) - %s" % payload, "error")
                self._update_button()
            elif kind == "closed":
                pass

    @staticmethod
    def _explain_open_error(msg):
        low = msg.lower()
        if "access" in low or "denied" in low or "busy" in low or "permission" in low:
            return "Port is busy - close PuTTY / the serialterminal tab, then Connect again"
        return "Cannot open port - %s" % msg

    def _redraw_if_needed(self):
        now = self.game_now()
        key = (self.game.version, self.game.view_phase(now), self.conn, id(self.game))
        if key == self.last_key:
            return
        self.last_key = key
        self.canvas.delete("all")
        render(Painter(self.canvas, self.S), self.game, now, self.conn)

    def on_close(self):
        self._stop_source()
        self.root.destroy()


# ================================================================== main
def parse_args(argv=None):
    ap = argparse.ArgumentParser(description="Memory Pattern Master - PC display")
    ap.add_argument("--port", help="เช่น COM12 (ถ้าไม่ระบุ จะหาพอร์ต ST-Link ให้เอง)")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument("--demo", action="store_true", help="เล่นตัวอย่างโดยไม่ต้องมีบอร์ด")
    ap.add_argument("--speed", type=float, default=2.0, help="ความเร็วของ --demo (ค่าเริ่มต้น 2)")
    ap.add_argument("--no-auto", action="store_true", help="ไม่ต่อพอร์ตอัตโนมัติ")
    return ap.parse_args(argv)


def _enable_dpi_awareness():
    if not sys.platform.startswith("win"):
        return
    try:
        import ctypes
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except Exception:
            ctypes.windll.user32.SetProcessDPIAware()
    except Exception:
        pass


def main(argv=None):
    args = parse_args(argv)
    if tk is None:
        print("tkinter not found. On Windows reinstall Python with 'tcl/tk' ticked; "
              "on Linux: sudo apt install python3-tk")
        return 1
    _enable_dpi_awareness()
    root = tk.Tk()
    App(root, args)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
