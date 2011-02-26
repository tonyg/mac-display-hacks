wh = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
hh = [0.8, 0.8, 0.8, 0.8, 0.3, 0.3, 0.3]

xoffsets = [-1, 1]

erosionk = 0.5

def i():
    global wh, hh
    nwh = list(wh)
    nhh = list(hh)
    print '--------------'
    print '   ', ' ', ', '.join('%010.6f' % (vv+ww,) for (vv,ww) in zip(nwh,nhh))
    print '   ', ' ', ', '.join('%010.6f' % (vv,) for vv in nwh), sum(nwh)
    print '   ', ' ', ', '.join('%010.6f' % (vv,) for vv in nhh), sum(nhh)
    print '--------------'
    for x in range(len(wh)):
        #print '>>>',   x, ', '.join('%010.6f' % (vv,) for vv in nwh), sum(nwh)
        #print '   ', ' ', ', '.join('%010.6f' % (vv,) for vv in nhh), sum(nhh)

        v = [False, False]
        count = 0

        for i in range(len(xoffsets)):
            x1 = x + xoffsets[i]
            if x1 >= 0 and x1 < len(wh):
                dh = (wh[x] + hh[x]) - (wh[x1] + hh[x1])
                if dh > 0:
                    v[i] = dh
                    count = count + dh

        if count:
            q = wh[x] * 0.01
            for i in range(len(xoffsets)):
                if v[i]:
                    x1 = x + xoffsets[i]
                    nhh[x1] += q * v[i] / count * erosionk
                    nwh[x1] += q * v[i] / count
            nhh[x] -= q * erosionk
            nwh[x] -= q
    wh = nwh
    hh = nhh

    hh = nhh

for n in range(10000):
    i()



# def i():
#     global wh, hh
#     nwh = list(wh)
#     nhh = list(hh)
#     print '--------------'
#     print '   ', ' ', ', '.join('%010.6g' % (vv,) for vv in nwh)
#     print '   ', ' ', ', '.join('%010.6g' % (vv,) for vv in nhh)
#     print '--------------'
#     for x in range(len(wh)):
#         print '>>>',   x, ', '.join('%010.6g' % (vv,) for vv in nwh)
#         print '   ', ' ', ', '.join('%010.6g' % (vv,) for vv in nhh)

#         v = [False, False]
#         count = 0

#         for i in range(len(xoffsets)):
#             x1 = x + xoffsets[i]
#             if x1 >= 0 and x1 < len(wh):
#                 if hh[x1] < wh[x] + hh[x]:
#                     v[i] = True
#                     count = count + 1

#         if count:
#             whs = wh[x] / count
#             for i in range(len(xoffsets)):
#                 if v[i]:
#                     x1 = x + xoffsets[i]
#                     nhh[x1] += whs * 0.1
#                     nwh[x1] += whs
#             nhh[x] -= wh[x] * 0.1
#             nwh[x] -= wh[x]
#     wh = nwh
