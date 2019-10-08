

#define DRIVER_NAME "caninos-usb2phy"
#define DRIVER_DESC "Caninos Labrador USB2 Phy Driver"

struct caninos_usb_phy
{
	struct usb_phy phy;
	struct device *dev;
	
	
	int id;
	void __iomem *base;
};

static const struct of_device_id caninos_usb_phy_id_table[] = {
	{ .compatible = "caninos,k7-usb2phy-0", .data = (void*)(0) },
	{ .compatible = "caninos,k7-usb2phy-1", .data = (void*)(1) },
	{ },
};

MODULE_DEVICE_TABLE(of, caninos_usb_phy_id_table);

static int caninos_usb_phy_probe(struct platform_device *pdev)
{
	struct caninos_usb_phy *caninos_phy = NULL;
	const struct of_device_id *match;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	enum usb_phy_interface phy_type;
	int err;
	
	caninos_phy = devm_kzalloc(&pdev->dev, sizeof(*caninos_phy), GFP_KERNEL);
	
	if (!caninos_phy)
	{
		return -ENOMEM;
	}
	
	match = of_match_device(caninos_usb_phy_id_table, &pdev->dev);
	
	if (!match)
	{
		dev_err(dev, "no device match found");
		return -ENODEV;
	}
	
	caninos_phy->port = (int)(match->data);
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res)
	{
		dev_err(dev, "failed to get I/O memory\n");
		return -ENXIO;
	}
	
	caninos_phy->base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (!caninos_phy->base)
	{
		dev_err(&dev, "Failed to remap I/O memory\n");
		return -ENOMEM;
	}
	
	phy_type = of_usb_get_phy_mode(np);
	
	platform_set_drvdata(pdev, caninos_phy);
	
	
	
	caninos_phy->dev = dev;
	caninos_phy->phy.dev = caninos_phy->dev;
	caninos_phy->phy.label = DRIVER_NAME;
	caninos_phy->phy.type = USB_PHY_TYPE_USB2;
	
	
	
	
	
	
	
	
	
	
	/*switch (phy_type) {
	case USBPHY_INTERFACE_MODE_UTMI:
		err = utmi_phy_probe(tegra_phy, pdev);
		if (err < 0)
			return err;
		break;

	case USBPHY_INTERFACE_MODE_ULPI:
		tegra_phy->is_ulpi_phy = true;

		tegra_phy->reset_gpio =
			of_get_named_gpio(np, "nvidia,phy-reset-gpio", 0);
		if (!gpio_is_valid(tegra_phy->reset_gpio)) {
			dev_err(&pdev->dev,
				"Invalid GPIO: %d\n", tegra_phy->reset_gpio);
			return tegra_phy->reset_gpio;
		}
		tegra_phy->config = NULL;
		break;

	default:
		dev_err(&pdev->dev, "phy_type %u is invalid or unsupported\n",
			phy_type);
		return -EINVAL;
	}
	
	

	if (of_find_property(np, "dr_mode", NULL))
		tegra_phy->mode = usb_get_dr_mode(&pdev->dev);
	else
		tegra_phy->mode = USB_DR_MODE_HOST;
	
	tegra_phy->u_phy.dev = &pdev->dev;
	
	err = tegra_usb_phy_init(tegra_phy);
	
	if (err < 0)
		return err;

	tegra_phy->u_phy.set_suspend = tegra_usb_phy_suspend;
	
	*/
	
	
	
	
	err = usb_add_phy_dev(&tegra_phy->u_phy);
	
	if (err < 0)
	{
		///tegra_usb_phy_close(tegra_phy);
		return err;
	}
	
	return 0;
}

static int caninos_usb_phy_remove(struct platform_device *pdev)
{
	struct tegra_usb_phy *tegra_phy = platform_get_drvdata(pdev);

	usb_remove_phy(&tegra_phy->u_phy);
	tegra_usb_phy_close(tegra_phy);

	return 0;
}

static struct platform_driver caninos_usb_phy_driver = {
	.probe		= caninos_usb_phy_probe,
	.remove		= caninos_usb_phy_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table = caninos_usb_phy_id_table,
	},
};
module_platform_driver(caninos_usb_phy_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

