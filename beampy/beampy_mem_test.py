import unittest
import beampy
import numpy as np

JAI = beampy.jpy.get_class('javax.media.jai.JAI')
JAI.getDefaultInstance().getTileCache().setMemoryCapacity(256 * 1000 * 1000)

test_product_file = './MER_RR__1P.N1'

class TestBeamIO(unittest.TestCase):


    def test_readProduct_mem(self):
        N = 365
        print('test_readProduct_mem')
        for i in range(N):
            print('i = ', i)
            product = beampy.ProductIO.readProduct(test_product_file)
            product.dispose()


    def test_readProduct_and_readPixels_mem(self):
        N = 365
        print('test_readProduct_and_readPixels_mem')
        for i in range(N):
            print('i = ', i)
            product = beampy.ProductIO.readProduct(test_product_file)
            band = product.getBand('radiance_9')
            w = band.getRasterWidth()
            h = band.getRasterHeight()
            a = np.zeros(w, dtype=np.float32)

            for y in range(h):
                band.readPixels(0, 0, w, 1, a)

            product.dispose()


if __name__ == '__main__':
    unittest.main()