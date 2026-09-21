---
version: v1.12.121
date: 2026-09-21
area: UI / Griswold's shop
tests: 832/832
---

# Griswold's service buttons clear the grid frame by four pixels

## The ask

> move the buttons 4px above grid frame.

## The arithmetic, and why the frame's top was re-checked

The framed canvas begins its ornate band at **y 159**, and a button is 34 tall, so four pixels of air puts the
row at **y 121**: 121..154 of button, 155..158 of air, 159 of frame. It was 128, which was flush against the
guide canvas and three pixels into the frame on this one.

The top edge was sampled at **nine columns across the row's whole span** (x 30 to 310) rather than at the one
column the overlap was first noticed in. It is level at 159 everywhere, so one y serves all six.

## Moved, not cropped

The inventory's tab row was fixed four versions ago by cropping it, and that answer does not transfer: those
are procedurally drawn plates, while these are the user's painted 34x34 `002.Button.png` and a crop would have
cut through the frame drawn into the art itself.

## Two more overlaps at the FOOT of the same frame

The bottom band ends at **y 628**, and two things the guide canvas placed now run into it:

| Element | Sits at | Overlap |
| --- | --- | --- |
| The gold icon | y 627..654 | 2 px |
| The Refresh-until button | y 627..660 | 2 px |

Both were measured off the guide canvas, which carried no frame - correct then, colliding now. By the rule just
given, four pixels clear of the frame's foot is **y 633** for both.

Not moved. The user positioned both deliberately and the gold was taken from their own guide; the same courtesy
that kept the button row from being cropped applies to moving art they placed. Put to them with the numbers.

## Build

Debug, clean. 832/832.

## Not verified

Not seen on screen. The two overlaps above are known; what to check is that four pixels reads as deliberate air
rather than as a misalignment.
