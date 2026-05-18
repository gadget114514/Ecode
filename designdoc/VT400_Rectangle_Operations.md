# VT400 Rectangle Operations

## DECCRA — Copy Rectangular Area

    CSI Pts;Pls;Pbs;Prs;Pps;Ptd;Pld;Ppd $v

| Parameter | Description |
|-----------|-------------|
| Pts | top-line border (source) |
| Pls | left-column border (source) |
| Pbs | bottom-line border (source) |
| Prs | right-column border (source) |
| Pps | page number (source) |
| Ptd | top-line border (destination) |
| Pld | left-column border (destination) |
| Ppd | page number (destination) |

---

## DECERA — Erase Rectangular Area

    CSI Pt;Pl;Pb;Pr $z

| Parameter | Description |
|-----------|-------------|
| Pt | top-line border |
| Pl | left-column border |
| Pb | bottom-line border |
| Pr | right-column border |

---

## DECFRA — Fill Rectangular Area

    CSI Pch;Pt;Pl;Pb;Pr $x

| Parameter | Description |
|-----------|-------------|
| Pch | decimal code of fill character |
| Pt | top-line border |
| Pl | left-column border |
| Pb | bottom-line border |
| Pr | right-column border |

Note: Pch is the **first** parameter.

---

## DECSERA — Selective Erase Rectangular Area

    CSI Pt;Pl;Pb;Pr ${
    
| Parameter | Description |
|-----------|-------------|
| Pt | top-line border |
| Pl | left-column border |
| Pb | bottom-line border |
| Pr | right-column border |

---

## DECSACE — Select Attribute Change Extent

    CSI Ps *x

| Ps | Description |
|----|-------------|
| 0 or 1 | stream of character positions |
| 2 | rectangular area of character positions |
| 3 | line of character positions |

---

## DECCARA — Change Attributes in Rectangular Area

    CSI Pt;Pl;Pb;Pr;Ps1..Psn $r

| Parameter | Description |
|-----------|-------------|
| Pt | top-line border |
| Pl | left-column border |
| Pb | bottom-line border |
| Pr | right-column border |
| Psn | visual character attributes (SGR parameter values) |

---

## DECRARA — Reverse Attributes in Rectangular Area

    CSI Pt;Pl;Pb;Pr;Ps1..Psn $t

| Parameter | Description |
|-----------|-------------|
| Pt | top-line border |
| Pl | left-column border |
| Pb | bottom-line border |
| Pr | right-column border |
| Psn | visual character attributes (SGR parameter values) |
