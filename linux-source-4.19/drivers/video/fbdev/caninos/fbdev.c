
#define DRIVER_NAME "caninos-fb"
#define pr_fmt(fmt) DRIVER_NAME": "fmt

#include <linux/platform_device.h>
#include <linux/platform_data/simplefb.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/parser.h>
#include <linux/module.h>

#define PSEUDO_PALETTE_SIZE 16

static int simplefb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			      u_int transp, struct fb_info *info)
{
	u32 *pal = info->pseudo_palette;
	u32 cr = red >> (16 - info->var.red.length);
	u32 cg = green >> (16 - info->var.green.length);
	u32 cb = blue >> (16 - info->var.blue.length);
	u32 value;

	if (regno >= PSEUDO_PALETTE_SIZE)
	{
		return -EINVAL;
	}

	value = (cr << info->var.red.offset) |
		(cg << info->var.green.offset) |
		(cb << info->var.blue.offset);
		
	if (info->var.transp.length > 0) {
		u32 mask = (1 << info->var.transp.length) - 1;
		mask <<= info->var.transp.offset;
		value |= mask;
	}
	pal[regno] = value;

	return 0;
}

struct simplefb_par;

static struct fb_ops simplefb_ops = {
	.owner          = THIS_MODULE,
	.fb_setcolreg	= simplefb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};



struct simplefb_params {
	u32 width;
	u32 height;
	u32 stride;
	struct simplefb_format *format;
};

struct simplefb_par {
	u32 palette[PSEUDO_PALETTE_SIZE];
};

static struct simplefb_format myformat = {
	.name = "a8r8g8b8",
	.bits_per_pixel = 32,
	.red = {16, 8}, 
	.green = {8, 8},
	.blue = {0, 8},
	.transp = {24, 8},
	.fourcc = DRM_FORMAT_ARGB8888,
};

static int simplefb_probe(struct platform_device *pdev)
{
	struct simplefb_params params;
	struct simplefb_par *par;
	struct resource fbmem;
	struct fb_info *info;
	int ret;
	
	struct device_node *np;
	
	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	
	if (!np)
	{
		pr_err("No memory-region node at Device Tree.\n");
		return -ENOMEM;
	}
	
	ret = of_address_to_resource(np, 0, &fbmem);
	
	if (ret)
	{
		pr_err("Could not get memory reserved for framebuffer.\n");
		return ret;
	}
	
	params.width  = 1920;
	params.height = 1080;
	params.stride = 1920 * 4;
	params.format = &myformat;
	
	info = framebuffer_alloc(sizeof(struct simplefb_par), &pdev->dev);
	
	if (!info)
	{
		pr_err("Could not alloc framebuffer.\n");
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, info);
	
	par = info->par;
	
	strncpy(info->fix.id, "caninosfb", sizeof(info->fix.id));
	
	info->fix.smem_start  = fbmem.start;
	info->fix.smem_len    = resource_size(&fbmem);
	info->fix.line_length = params.stride;
	info->fix.type        = FB_TYPE_PACKED_PIXELS;
	info->fix.visual      = FB_VISUAL_TRUECOLOR,
	info->fix.accel       = FB_ACCEL_NONE,
	
	info->var.activate       = FB_ACTIVATE_NOW;
	info->var.vmode          = FB_VMODE_NONINTERLACED;
	info->var.height         = -1;
	info->var.width          = -1;
	info->var.xres           = params.width;
	info->var.yres           = params.height;
	info->var.xres_virtual   = params.width;
	info->var.yres_virtual   = params.height;
	info->var.bits_per_pixel = params.format->bits_per_pixel;
	
	info->var.red    = params.format->red;
	info->var.green  = params.format->green;
	info->var.blue   = params.format->blue;
	info->var.transp = params.format->transp;
	
	
	info->apertures = alloc_apertures(1);
	
	if (!info->apertures)
	{
		pr_err("Could not alloc framebuffer apertures.\n");
		framebuffer_release(info);
		return -ENOMEM;
	}
	
	info->apertures->ranges[0].base = info->fix.smem_start;
	info->apertures->ranges[0].size = info->fix.smem_len;
	
	info->fbops = &simplefb_ops;
	info->flags = FBINFO_DEFAULT | FBINFO_MISC_FIRMWARE;
	
	
	
	info->screen_base = 
		devm_ioremap_wc(&pdev->dev, info->fix.smem_start, info->fix.smem_len);
	
	if (!info->screen_base)
	{
		pr_err("Could not map framebuffer address.\n");
		framebuffer_release(info);
		return -ENOMEM;
	}
	
	info->pseudo_palette = par->palette;
	
	ret = register_framebuffer(info);
	
	if (ret)
	{
		pr_err("Unable to register framebuffer.\n");
		return ret;
	}
	
	pr_info("Driver probe successfully finished.\n");

	return 0;
}

static const struct of_device_id simplefb_of_match[] = {
	{ .compatible = "caninos,k7-fb", },
	{ },
};
MODULE_DEVICE_TABLE(of, simplefb_of_match);

static struct platform_driver simplefb_driver = {
	.probe = simplefb_probe,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = simplefb_of_match,
	},
};

static int __init caninosfb_init(void)
{
	return platform_driver_register(&simplefb_driver);
}

fs_initcall(caninosfb_init);

